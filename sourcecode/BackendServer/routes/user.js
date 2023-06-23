const express = require('express');
const { check, validationResult } = require('express-validator');
const service = require('../services/service');
const utils = require('../utils');
const jwt = require('jsonwebtoken');
const { auth, passwordAuth } = require('../middleware/auth');

const router = express.Router();

/* GET users. 
 * example: curl http://localhost:3000/users?page=5
 * */
router.get('/all', auth, async function(req, res, next) {
	try {
		await service.getAllUsers(
			res,
			req.query.page
		);
	} catch (err) {
		console.error(`Error while getting users`, err.message);
		next(err);
	}
});

// Get information of 1 user, a user can not get information of other users -> need to validate token and user id
router.get('/me', auth, async function(req, res, next) {
	try {
		const authHeader = req.headers.authorization;
		const token = authHeader && authHeader.split(' ')[1];

		const decodedToken = await jwt.verify(token, process.env.TOKEN_SECRET);

		await service.getUser(
			res,
			decodedToken.user_id
		);
	} catch (err) {
		console.error(`Error while getting users`, err.message);
		next(err);
	}
});

router.post('/signup', 
			[
			check("email").not().isEmpty().withMessage("Email is required").bail().isEmail().withMessage("Invalid email"),
			check("first_name").not().isEmpty(),
			check("last_name").not().isEmpty(),
			check('confirm_password', 'Passwords do not match').custom((value, {req}) => (value === req.body.password)),
			check("password").not().isEmpty().custom((value, {req}) => (utils.validatePasswordComplexity(value))).withMessage("Password complexity does not meet requirements!"),
			check('ip_address', 'Invalid IP address').not().isEmpty().custom((value, {req}) => (utils.validateIpAddress(value))),
			], async function(req, res, next) {

	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		return res.status(422).jsonp(errors.array());
	}

	try {
		await service.signup(
			res,
			req.body
		);
	} catch (err) {
		console.error(`Error while getting users`, err.message);
		next(err);
	}
});

router.post('/deactivate', auth, async function(req, res, next) {
	try {
		await service.deactivateUser(
			res, 
			req.body.user_id
		);
	} catch (err) {
		console.error(`Error while getting users`, err.message);
		next(err);
	}
});

router.get('/:id/verify/:token', async function(req, res, next) {
	try {
		await service.activateUser(
			res,
			req.params.id,
			req.params.token
		);
	} catch (err) {
		console.error(`Error while activating user`, err.message);
		next(err);
	}
});

router.get('/:id/calls', auth, async function(req, res, next) {

	try {
		await service.getCallList(
			res,
			req.params.id,
			req.query.page
		);
	} catch (err) {
		console.error(`Error while updating email`, err.message);
		next(err);
	}

});

router.post('/:id/start-call',
			auth,
			[
				check("to_contact").not().isEmpty(),
			],
			async function(req, res, next) {

	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		return res.status(422).jsonp(errors.array());
	}

	try {
		await service.startCall(
			res,
			req.params.id,
			{
				toContact: req.body.to_contact
			}
		);
	} catch (err) {
		console.error(`Error while start call`, err.message);
		next(err);
	}
});

router.post('/:id/stop-call',
			auth,
			[
				check("to_contact").not().isEmpty(),
				check("call_status").not().isEmpty().isInt({min: 0, max: 3}),
				check("started_at").not().isEmpty().isISO8601(),
				check("stopped_at").not().isEmpty().isISO8601(),
			],
			async function(req, res, next) {

	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		return res.status(422).jsonp(errors.array());
	}

	try {
		await service.stopCall(
			res,
			req.params.id, 
			{
				toContact: req.body.to_contact,
				callStatus: req.body.call_status,
				startedAt: req.body.started_at,
				stoppedAt: req.body.stopped_at
			}
		);
	} catch (err) {
		console.error(`Error while updating email`, err.message);
		next(err);
	}

});

router.post('/check-email', [check("email").not().isEmpty()], async function(req, res, next) {

	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		console.log(errors);
		return res.status(422).jsonp(errors.array());
	}

	try {
		await service.checkEmail(
			res,
			req.body.email
		);
	} catch (err) {
		console.error(`Error while checking email`, err.message);
		next(err);
	}
})

router.post('/update',
			auth,
			[
				check("otp").not().isEmpty().isInt({min: 0, max: 999999}),
				check("current_password").not().isEmpty(),
				check("new_password").optional().custom((value, {req}) => {
					if (value !== req.body.confirm_new_password) return false;
					return utils.validatePasswordComplexity(value);
				}),
				check("email").optional().isEmail(),
			],
			async function(req, res, next) {

	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		console.log(errors);
		return res.status(422).jsonp(errors.array());
	}

	try {
		const authHeader = req.headers.authorization;
		const token = authHeader && authHeader.split(' ')[1];

		const decodedToken = await jwt.verify(token, process.env.TOKEN_SECRET);

		await service.updateUser(
			res, 
			decodedToken.user_id,
			{
				otp: req.body.otp,
				currentPassword: req.body.current_password,
				newPassword: req.body.new_password,
				confirmNewPassword: req.body.confirm_new_password,
				newEmail: req.body.new_email
			}
		);
	} catch (err) {
		console.error(`Error while generating OTP`, err.message);
		next(err);
	}
});

router.post('/get-info-from-ip', 
			auth,
			[
				check('ip_address', 'Invalid IP address').not().isEmpty().custom((value, {req}) => (utils.validateIpAddress(value))),
			],
			async function(req, res, next) {

	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		console.log(errors);
		return res.status(422).jsonp(errors.array());
	}

	try {
		await service.getInfoFromIp(
			res, 
			{
				ipAddress: req.body.ip_address,
			}
		);
	} catch (err) {
		console.error(`Error while get information from IP address`, err.message);
		next(err);
	}
});

router.get('/generate-otp',
			auth,
			async function(req, res, next) {
	try {
		const authHeader = req.headers.authorization;
		const token = authHeader && authHeader.split(' ')[1];

		if (token) {
			const decodedToken = await jwt.verify(token, process.env.TOKEN_SECRET);
			await service.generateAndSendOTPById(
				res,
				decodedToken.user_id
			);
		}

	} catch (err) {
		console.log("Error while generating OTP", err.message);
		next(err);
	}
});

router.post('/generate-otp',
			[
				check("email").not().isEmpty(),
				check("password").not().isEmpty()
			],
			passwordAuth,
			async function(req, res, next) {

	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		console.log(errors);
		return res.status(422).jsonp(errors.array());
	}

	try {
		await service.generateAndSendOTPByEmail(
			res,
			req.body.email
		);

	} catch (err) {
		console.log("Error while generating OTP", err.message);
		next(err);
	}
	


	
});

router.post('/reset-password',
			[
                check("email").not().isEmpty().isEmail(),
				check("otp").not().isEmpty().isInt({min: 0, max: 999999}),
                check("password").not().isEmpty().custom((value, {req}) => (
					utils.validatePasswordComplexity(value)
				)).withMessage("Password complexity does not meet requirements!"),
                check('confirm_password', 'Passwords do not match').custom((value, {req}) => (value === req.body.password))
			],
			async function(req, res, next) {

	const errors = validationResult(req);
	if (!errors.isEmpty()) {
			return res.status(422).jsonp(errors.array());
	}

	try {
			await service.resetPassword(
				res,
				{
					email: req.body.email,
					otp: req.body.otp,
					password: req.body.password,
				}
			);

	} catch (err) {
			console.error(`Error while updating email`, err.message);
			next(err);
	}

});

module.exports = router;
