const { body } = require('express-validator');
const bcrypt = require("bcrypt");
const jwt = require('jsonwebtoken');
const dotenv = require('dotenv');

const db = require('./db');
const utils = require('../utils');
const config = require('../config');

dotenv.config();

const userTable = "contact";
const authTable = "auth";
const callTable = "conversation";

async function getAllUsers(res, page = 1) {
	const offset = utils.getOffset(page, config.listPerPage);
	const rows = await db.query(
		`SELECT contact_id, email, last_name, first_name, ip_address
		FROM ${userTable} LIMIT ${offset}, ${config.listPerPage}`
	);
	// const data = utils.emptyOrRows(rows);
	// const meta = {page};
	//
	let data = null;
	let response = {};
	for(let i in rows) {
		data = "";
		data += rows[i].email;
		data += ",";
		data += rows[i].last_name;
		data += ",";
		data += rows[i].first_name;
		data += ",";
		data += rows[i].ip_address;

		response[parseInt(i)+1] = data;
	}

	console.log(response);

	return res.status(200).json(response);
}

async function getUser(res, id) {
	const row = await db.query(
		`SELECT contact_id, email, last_name, first_name, ip_address, password
		FROM ${userTable} WHERE contact_id = ?`,
		[id]
	);
	return res.status(200).json(row[0]);
}

async function signup(res, user) {
	let message = null;
	let statusCode = null;

	const hashedPassword = await bcrypt.hash(user.password, 6);

	const passwordExpiredAt = new Date(new Date().setDate(new Date().getDate() + 30));

	const duplicateResult = await db.query(
		`SELECT 1 FROM ${userTable} WHERE email = ? OR ip_address = ?`,
		[user.email, user.ip_address]
	);
	if (duplicateResult && duplicateResult.length > 0) return res.status(409).json({message: "Duplicate entry"});

	const result = await db.query(
		`INSERT INTO ${userTable}
		(email, first_name, last_name, password, ip_address, password_expired_at)
		VALUES
		(?, ?, ?, ?, ?, ?)`,
		[user.email, user.first_name, user.last_name, hashedPassword, user.ip_address, passwordExpiredAt]
	);

	if (result && result.affectedRows) {
		const userResult = await db.query(
			`SELECT contact_id, email, first_name from ${userTable}
			WHERE email = ?`,
			[user.email]
		);
		
		await db.query(
			`INSERT INTO ${authTable}
			(contact_id)
			VALUES
			(?)`,
			[userResult[0].contact_id]
		);

		utils.sendActivationEmail({dst: userResult[0].email, userId: userResult[0].contact_id, name: userResult[0].first_name});

		message = 'User created successfully. Please check your email to activate your account!';
		statusCode = 200;

	} else {
		message = 'Error creating user!';
		statusCode = 400;
	}

	return res.status(statusCode).json({message});
}

async function deactivateUser(res, userId) {
	let message = null;
	let statusCode = null;

	const query = `UPDATE ${userTable} 
				   SET is_active = false
				   WHERE contact_id = ?`
	const result = await db.query(query, [userId]);

	if (result.affectedRows) {
		message = 'User deactivated successfully';
		statusCode = 200;
	} else {
		message = 'Failed to deactivate user!';
		statusCode = 400;
	}

	return res.status(statusCode).json({message});
}

async function login(res, {email, password, otp}) {
	let message = null;
	let response = null;
	let statusCode = null;

	const query = `SELECT * FROM ${userTable} WHERE email = ?`;
	
	const result = await db.query(query, [email]);
	if (result && result.length === 1) {
		if (result[0].is_locked === 1) {
			const now = new Date();
			if (result[0].unlock_at < now && result[0].password_expired_at > now) {
				await db.query(
					`UPDATE ${userTable} SET is_locked = false WHERE contact_id = ?`,
					[result[0].contact_id]
				);
			} else {
				const remaining =  parseInt(
					((new Date(result[0].unlock_at)) - now) / (1000*60)
				) + 1;
				response = {
					message: "Your account is locked",
					unlock_after_min: remaining
				}

				return res.status(403).json(response);
			}
		}

		// Check password 
		const now = new Date();
		const isCorrect = bcrypt.compareSync(password, result[0].password);
		if (isCorrect === true) {
			// Password is correct
			if (result[0].is_active === 1) {
				// Password is correct and account is active
				if (result[0].password_expired_at < now) {
					db.lockAccount(result[0]);
					await db.query(
						`UPDATE ${userTable} SET is_locked = true WHERE contact_id = ?`,
						[result[0].contact_id]
					);

					resposne = {
						message: "Password is expired. Please reset your password"
					}
					statusCode = 403;

				} else {
					const rc = await verifyOTP(result[0], otp);
					if (rc) {
						const accessToken = utils.generateAccessToken(result[0].contact_id);

						response = {
							message: "Login successfully",
							access_token: accessToken
						}
						statusCode = 200;

						return res.status(statusCode).json(response);
					} else {
						const failedAttempts = await db.increaseFailedAttempt(result[0]);
						let remainingAttempts = 0;
						if (parseInt(process.env.FAILED_LOGIN_THRESHOLD) >= failedAttempts) {
							remainingAttempts = parseInt(process.env.FAILED_LOGIN_THRESHOLD) - failedAttempts;
						}

						response = {
							message: "Invalid credentials",
							remaining_attempts: remainingAttempts
						}
						statusCode = 401;
					}

				}
			} else {
				response = {
					message: "Account is not activated"
				}
				statusCode = 403;
			}
		} else {
			// Wrong password, check failed attempts
			statusCode = 401

			const failedAttempts = await db.increaseFailedAttempt(result[0]);
			let remainingAttempts = 0;
			if (parseInt(process.env.FAILED_LOGIN_THRESHOLD) >= failedAttempts)  {
				remainingAttempts = parseInt(process.env.FAILED_LOGIN_THRESHOLD) - failedAttempts;

				response = {
					message: "Invalid credentials",
					remaining_attempts: remainingAttempts
				}
			} else {
				const updatedResult = await db.query(
                                        `SELECT unlock_at FROM ${userTable} WHERE contact_id = ?`,
                                        [result[0].contact_id]
                                );

				const remaining =  parseInt(
					((new Date(updatedResult[0].unlock_at)) - now) / (1000*60)
				) + 1;

				response = {
					message: "Your account is locked",
					unlock_after_min: remaining
				}
			}
		}

	} else {
		response = {
			message: "Unable to find user"
		}
		statusCode = 401;
	}

	return res.status(statusCode).json(response);
}

async function getAccessToken(res, userId, refreshToken) {
	let message = null;

	try {
		const decodedToken = await jwt.verify(refreshToken, process.env.TOKEN_SECRET);
		const tokenFromRedis = await db.getToken(userId);
		if (decodedToken && decodedToken.user_id === userId && refreshToken === tokenFromRedis) {
			const accessToken = await utils.generateAccessToken(userId);
			return res.status(200).json({refreshToken, accessToken});
		} else {
			message = "Invalid request";
			return res.status(400).json({message});
		}

	} catch (err) {
		if (err.name ===  'TokenExpiredError') {
			message = "Token expired"
			return res.status(403).json({message});
		} else {
			console.log(err);
			message = "Unknown error";
			return res.status(401).json({message});
		}
	} 
}

async function activateUser(res, userId, token) {
	let message = "Failed to activate user";
	let statusCode = 400;

	try {
		const decodedToken = await jwt.verify(token, process.env.TOKEN_SECRET);
		if (decodedToken.user_id === userId) {
			const result = await db.query(
				`UPDATE ${userTable} SET is_active = true WHERE contact_id = ?`,
				[userId]
			);

			if (result && result.affectedRows) {
				message = "Activated user successfully";
				statusCode = 200;
			}
		}

		return res.status(statusCode).json({message});

	} catch (err) {
		console.log(err);
		return res.status(statusCode).json(err);
	};
}

async function verifyOTP(user, otp) {
	let message = null;
	let statusCode = null;

	const now = new Date();

	const otpResult = await db.query(
		`SELECT 1 FROM auth WHERE contact_id = ? AND otp_used = false AND otp = ? AND expired_at > ?`,
		[user.contact_id, otp, now]
	);

	if (otpResult && otpResult.length === 1) {
		// Set OTP as used
		db.query(
			`UPDATE ${authTable} SET otp_used = true WHERE contact_id = ?`, 
			[user.contact_id]
		);

		await db.query(
			`UPDATE ${authTable} SET failed_attempt = 0 WHERE contact_id = ?`, 
			[user.contact_id]
		);

		message = "Successfully login";
		statusCode = 200;

		return true;

	}

	return false;
}

async function requestUpdateEmail(res, userId) {
	let message = null;
	let statusCode = null;

	const result = await db.query(
		`SELECT contact_id, email, password, first_name FROM ${userTable} WHERE contact_id = ? AND is_active = true AND is_locked = false`,
		[userId]
	);

	if (!result || result.length !== 1) {
		return res.status(400).json("User not found");
	}

	const confirmationCode = utils.generateOTP();
	db.storeOTP(result[0].contact_id, confirmationCode, process.env.UPDATE_EMAIL_OTP_DURATION_MIN);
	utils.sendEmailUpdateConfirmation({dst: result[0].email, name: result[0].first_name, confirmationCode: confirmationCode});

	message = "Confirmation code is sent to the new email"
	statusCode = 200

	return res.status(statusCode).json({message});
}

async function requestResetPassword(res, email) {
	// Only user with valid login token can enter here
	let response = null;
	let message = null;

	const userResult = await db.query(
		`SELECT * FROM ${userTable} WHERE email = ? AND is_active = true`,
		[email]
	);

	if (userResult && userResult.length === 1) {
		const resetToken = utils.generateResetPasswordToken(userResult[0].contact_id);

		message = "Please check your email to get the confirmation code";
		statusCode = 200;

		const confirmationCode = utils.generateOTP();
		await db.storeOTP(userResult[0].contact_id, confirmationCode, process.env.RESET_PASSWORD_OTP_DURATION_MIN);
		utils.sendResetPasswordEmail({dst: email, name: userResult[0].first_name, confirmationCode: confirmationCode});

		response = {message, reset_token: resetToken};
	} else {
		message = "Email does not exist";
		statusCode = 400;

		response = {message}
	}

	return res.status(statusCode).json(response);
}

async function resetPassword(res, {email, password, otp}) {
	// Only user with valid login token can enter here
	const now = new Date();
	let response = null;
	let message = null;
	
	const userResult = await db.query(
		`SELECT * FROM ${userTable} WHERE email = ?`,
		[email]
	);

	message = "Unable to find user";
	if (!userResult || userResult.length !== 1) return res.status(400).json({message});

	const rc = await verifyOTP(userResult[0], otp);
	if (rc) {
		const hashedPassword = bcrypt.hash(password, 6);
		const passwordExpiredAt = new Date(new Date().setDate(new Date().getDate() + 30));

		const result = await db.query(
			`UPDATE ${userTable} SET password = ?, password_expired_at = ?, is_locked = false WHERE email = ?`,
			[hashedPassword, passwordExpiredAt, email]
		);

		if (result && result.affectedRows) {
			db.query(
				`UPDATE ${authTable} SET failed_attempt = 0 WHERE contact_id = ?`,
				[userResult[0].contact_id]
			);

			message = "Successfully changed password";
			statusCode = 200;
			
		} else {
			message = "Failed to change password";
			statusCode = 400;
		}

	} else {
		message = "Invalid credentials";
		statusCode = 401;
	}

	return res.status(statusCode).json({message});
}

async function startCall(res, userId, {toContact}) {
	let message = null;
	let statusCode = null;
	const now = new Date();

	try {
		const result = await db.query(
			`INSERT INTO ${callTable} (from_contact, to_contact, started_at) VALUES (?, ?, ?)`,
			[userId, toContact, now]
		);
		
		if (result && result.affectedRows === 1) {
			message = "Sucessfully created call";
			statusCode = 200;
		} else {
			message = "Failed to create call";
			statusCode= 500;
		}
	} catch (err) {
		console.log(err);
		message = "Failed to create call";
		statusCode= 400;
	}

	return res.status(statusCode).json({message});
}

async function stopCall(res, userId, {toContact, startedAt, stoppedAt, callStatus}) {
	if (callStatus == utils.REJECTED || callStatus == utils.MISSED) {
		stoppedAt = startedAt;
	}

	try {
		const result = await db.query(
			`UPDATE ${callTable} SET status = ?, stopped_at = ? WHERE from_contact = ? AND to_contact = ? AND started_at = ?`,
			[callStatus, stoppedAt, userId, toContact, startedAt]
		);
		
		if (result && result.afftedRows == 1) {
			message = "Sucessfully stop call";
			statusCode = 200;
		} else {
			message = "Failed to stop call";
			statusCode= 500;
		}
	} catch (err) {
		console.log(err);
		message = "Failed to stop call";
		statusCode= 400;
	}

	return res.status(statusCode).json({message});
}


async function getCallList(res, userId, page = 1) {
	let message = null;

	try {
		const offset = utils.getOffset(page, config.listPerPage);
		const result = await db.query(
			`SELECT * FROM ${callTable} WHERE from_contact = '${userId}' ORDER BY started_at DESC LIMIT ${offset}, ${config.listPerPage}`
		);

		data = utils.emptyOrRows(result);
		meta = {page};

		return res.status(200).json({data, meta});

	} catch (err) {
		console.log(err);
		message = "Failed to get call list";
		statusCode = 400;

		return res.status(statusCode).json({message});
	}
}

async function checkEmail(res, email) {
	let message = null;

	try {
		const result = await db.query(
			`SELECT 1 FROM ${userTable} WHERE email = '${email}'`
		);

		if (result && result.length === 1) {
			message = "Email existed";
			statusCode = 403;
		} else {
			message = "Email does not exist";
			statusCode = 200;
		}

	} catch (err) {
		console.log(err);
		message = "Error checking email";
		statusCode = 500;
	}

	return res.status(statusCode).json({message});
}

async function requestChangePassword(res, userId) {
	try {
		const result = await db.query(
			`SELECT * FROM ${userTable} WHERE contact_id = ? AND is_active = true AND is_locked = false`,
			[userId]
		);

		if (result && result.length === 1) {
			const confirmationCode = utils.generateOTP();
			db.storeOTP(result[0].contact_id, confirmationCode, process.env.CHANGE_PASSWORD_OTP_DURATION_MIN);

			utils.sendChangePasswordConfirmationEmail({dst: result[0].email, name: result[0].first_name, confirmationCode: confirmationCode});

			message = "Confirmation code has been sent to the email"
			statusCode = 200

		} else {
			message = "User does not exist";
			statusCode = 400;
		}

		return res.status(statusCode).json({message});

	} catch (err) {
		console.log(err);
	}
}

async function updateUser(res, userId, {currentPassword, newPassword, confirmNewPassword, otp, newEmail}) {
	let message = "User does not exist";
	let query = null;
	let changed = false;

	const userResult = await db.query(
		`SELECT * FROM ${userTable} WHERE contact_id = ?`,
		[userId]
	);

	if (!userResult || userResult.length !== 1) return res.status(400).json({message});

	message = "Invalid credentials";
	const isCorrect = bcrypt.compareSync(currentPassword, userResult[0].password);
	if (!isCorrect) return res.status(401).json({message});

	const now = new Date();
	const otpResult = await db.query(
		`SELECT 1 FROM auth WHERE contact_id = ? AND otp_used = false AND otp = ? AND expired_at > '${now}'`,
		[userId, otp]
	);

	if (otpResult && otpResult.length === 1) {
		// Set OTP as used
		db.query(
			`UPDATE ${authTable} SET otp_used = true WHERE contact_id = ?`,
			[userId]
		);

		if (newPassword && confirmNewPassword && newPassword === confirmNewPassword) {
			updatePassword(userResult[0], newPassword);
			changed = true;
		} 

		if (newEmail && newEmail !== userResult[0].email) {
			updateEmail(userResult[0], newEmail);
			changed = true;
		}

		if (changed) {
			statusCode = 200;
			message = "Updated user data";
		} else {
			message = "Invalid data";
			statusCode = 422;
		}

	} else {
		message = "Invalid OTP";
		statusCode = 400;
	}

	return res.status(statusCode).json({message});
}

async function updatePassword(user, newPassword) {
	const hashedPassword = await bcrypt.hash(newPassword, 6);

	const updateResult = await db.query(
		`UPDATE ${userTable} SET password = ? WHERE contact_id = ?`,
		[hashedPassword, user.contact_id]
	);

	if (updateResult.affectedRows) {
		// Remove refresh token
		// db.blacklistToken(result[0].contact_id);
		utils.sendPasswordChangedEmail({dst: user.email, name: user.first_name});

		return true;
	}

	return false;
}

async function updateEmail(user, newEmail) {
	const updateResult = await db.query(
		`UPDATE ${userTable} SET email = ? WHERE contact_id = ?`,
		[newEmail, user.contact_id]
	);

	if (updateResult.affectedRows) {
		// Remove refresh token
		// db.blacklistToken(userId);
		utils.sendEmailChangedEmail({dst: user.email, name: user.first_name, newEmail: newEmail});

		return true;
	}

	return false;
}


async function getInfoFromIp(res, { ipAddress }) {
	let response = null;
	let statusCode = null;

	const result = await db.query(
		`SELECT contact_id, email, first_name, last_name, ip_address from ${userTable} WHERE ip_address = ?`,
		[ipAddress]
	);

	if (result && result.length === 1) {
		response = result[0];
		statusCode = 200;

	} else {
		statusCode = 400;
		response = {
			message: "Unable to find user with this IP address",
		}
	}

	return res.status(statusCode).json(response);
}

async function generateAndSendOTPById(res, userId) {
	const otp = utils.generateOTP();
	console.log(otp);
	const result = await db.query(
		`SELECT email from ${userTable} WHERE contact_id = ?`,
		 [userId]
	);

	const rc = await db.storeOTP(userId, otp, process.env.UPDATE_DATA_OTP_DURATION_MIN);
	if (rc) {
		utils.sendOTPEmail({dst: result[0].email, otp: otp});
		statusCode = 200;
		message = "OTP has been sent to the email";
	} else {
		statusCode = 400;
		message = "Failed to generate OTP";
	}

	return res.status(statusCode).json({message});
}

async function generateAndSendOTPByEmail(res, email) {
	const otp = utils.generateOTP();
	console.log(otp);
	const result = await db.query(
		`SELECT contact_id, email from ${userTable} WHERE email = ?`,
		 [email]
	);

	if (result && result.length === 1) {
		const rc = await db.storeOTP(result[0].contact_id, otp, process.env.LOGIN_OTP_DURATION_MIN);
		if (rc) {
			utils.sendOTPEmail({dst: result[0].email, otp: otp});
			statusCode = 200;
			message = "OTP has been sent to the email";
		} else {
			statusCode = 400;
			message = "Failed to store OTP";
		}

		return res.status(statusCode).json({message});
	}

	message = "Email not found";
	statusCode = 400;

	return res.status(statusCode).json({message});

}

module.exports = {
	signup,
	activateUser,
	login,
	getAllUsers,
	getUser,
	updateUser,
	deactivateUser,
	getAccessToken,
	verifyOTP,
	resetPassword,
	getCallList,
	startCall,
	stopCall,
	checkEmail,
	requestChangePassword,
	updateEmail,
	getInfoFromIp,
	generateAndSendOTPById,
	generateAndSendOTPByEmail
}
