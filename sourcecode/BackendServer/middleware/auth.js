const jwt = require('jsonwebtoken');
const dotenv = require('dotenv');
const bcrypt = require('bcrypt');
const db = require('../services/db');
const utils = require('../utils');

dotenv.config();

const userTable = 'contact';

async function auth(req, res, next) {
	try {
		const now = new Date();

		const authHeader = req.headers.authorization;
		const token = authHeader && authHeader.split(' ')[1];

		if (token == null) throw 'No authorization header';
		
		const decodedToken = await jwt.verify(token, process.env.TOKEN_SECRET);
		const user_id = decodedToken.user_id;

		const result = await db.query(
			`SELECT is_active, is_locked, password_expired_at, unlock_at FROM ${userTable} WHERE contact_id = ?`,
			[user_id]
		);

		if (result && result.length === 1) {
			if (result[0].is_locked) {
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

					res.status(401).json(
						{
							message: "Your account is locked",
							unlock_after_min: remaining
						}
					);
				}
			} 

			if (!result[0].is_active) {
				res.status(401).json({message: "Your account is inactive"});
			}

			if (result[0].password_expired_at < now) {
				res.status(401).json({message: "Your password is expired. Please reset it"});
			}

			next();
		} else {
			throw 'User doest not exist!';
		}

	} catch (err) {
		if (err.name === 'TokenExpiredError') {
			res.status(401).json({
				error: 'Token expired'
			});
		} else {
			console.log(err);
			res.status(400).json({
				error: err
			});
			
		}
	}
};

async function passwordAuth(req, res, next) {
	try {
		const now = new Date();

		const result = await db.query(
			`SELECT * FROM ${userTable} WHERE email = ?`,
			[req.body.email]
		);

		if (result && result.length === 1) {
			if (result[0].is_locked) {
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

					return res.status(403).json(
						{
							message: "Your account is locked",
							unlock_after_min: remaining
						}
					);

				}
			} 

			if (!result[0].is_active) {
				return res.status(401).json({message: "Your account is inactive"});
			}

			if (result[0].password_expired_at < now) {
				return res.status(401).json({message: "Your password is expired. Please reset it"});
			}

			const isCorrect = bcrypt.compareSync(req.body.password, result[0].password);
			if (!isCorrect) {
				const failedAttempts = await db.increaseFailedAttempt(result[0]);
				let remainingAttempts = 0;
				if (process.env.FAILED_LOGIN_THRESHOLD > failedAttempts) {
					remainingAttempts = process.env.FAILED_LOGIN_THRESHOLD - failedAttempts;
				}
				response = {
					message: "Invalid credentials",
					remaining_attempts: remainingAttempts
				}

				return res.status(401).json(response);
			}

			next();
		} else {
			throw 'User doest not exist!';
		}

	} catch (err) {
		console.log(err);
		res.status(400).json({
			error: err
		});
	}
};

module.exports = {
	auth,
	passwordAuth
}
