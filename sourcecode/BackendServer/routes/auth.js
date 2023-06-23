const express = require('express');
const { check, validationResult } = require('express-validator');
const service = require('../services/service');
const utils = require('../utils');
const jwt = require('jsonwebtoken');
const { auth } = require('../middleware/auth');

const router = express.Router();

router.post('/login',
			[
				check("email", "Invalid email").not().isEmpty().isEmail(),
				check("password").not().isEmpty(),
				check("otp").not().isEmpty().isInt({min: 0, max: 999999})
			],
			async function(req, res, next) {

	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		return res.status(422).jsonp(errors.array());
	}

	try {
		await service.login(res, req.body);
	} catch (err) {
		console.log(err);
		console.error(`Error login`, err.message);
		next(err);
	}

});

router.post('/:id/refresh-token', [check("refresh_token").not().isEmpty()], async function(req, res, next) {
	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		return res.status(422).jsonp(errors.array());
	}

	try {
		await service.getAccessToken(res, req.params.id, req.body.refresh_token);
	} catch (err) {
		console.error(`Error while getting access token`, err.message);
		next(err);
	}
});

router.post('/verify-otp', [
		check("otp").not().isEmpty().isLength({min: 6, max: 6}).isInt().withMessage("OTP must be 6-digits number"),
		check("login_token").not().isEmpty().withMessage("Login token is required"),
		],
		async function(req, res, next) {

	const errors = validationResult(req);

	if (!errors.isEmpty()) {
		return res.status(422).jsonp(errors.array());
	}

	try {
		const decodedToken = await jwt.verify(req.body.login_token, process.env.TOKEN_SECRET);
		await service.verifyOTP(res, decodedToken.user_id, req.body.otp);

	} catch (err) {
		console.log(err);
		if (err.name === "TokenExpiredError") {
			const message = "Token expired";	
			return res.status(403).json({message});
		} else {
			console.error(`Error while verifying OTP`, err.message);
			next(err);
		}
	}
});

router.post('/request-reset-password', [
		check("email").not().isEmpty().isEmail()
		],
		async function(req, res, next) {

	const errors = validationResult(req);
	if (!errors.isEmpty()) {
		return res.status(422).jsonp(errors.array());
	}

	try {
		await service.requestResetPassword(res, req.body.email);
	} catch (err) {
		console.error(`Error while updating email`, err.message);
		next(err);
	}

});

router.post('/reset-password', [
		check("confirmation_code").not().isEmpty(),
		check("reset_token").not().isEmpty(),
		check("new_password").not().isEmpty().custom((value, {req}) => (utils.validatePasswordComplexity(value))).withMessage("Password complexity does not meet requirements!"),
		check('confirm_password', 'Passwords do not match').custom((value, {req}) => (value === req.body.new_password))
		], async function(req, res, next) {

	const errors = validationResult(req);
	if (!errors.isEmpty()) {
		return res.status(422).jsonp(errors.array());
	}

	try {
		const decodedToken = await jwt.verify(req.body.reset_token, process.env.TOKEN_SECRET);
		await service.resetPassword(res, {userId: decodedToken.user_id, password: req.body.new_password, otp: req.body.confirmation_code});
	} catch (err) {
		if (err.name === "TokenExpiredError") { 
			res.status(401).json({message: "Token expired"});
		} else {
			console.log(err);
			next(err);
		}
	}
});

module.exports = router;
