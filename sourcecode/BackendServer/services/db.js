const mysql = require('mysql2/promise');
const redis = require('redis');
const config = require('../config');
const utils = require('../utils');

const authTable = "auth";
const userTable = "contact";

/* MySQL */
async function query(sql, params) {
  const connection = await mysql.createConnection(config.db);
  // await connection.execute("SET time_zone='+00:00';");
  const [results, ] = await connection.execute(sql, params);

  return results;
}

async function createRedisConnection() {
                const client = redis.createClient();
                                client.on("connect", (err) => {
                                console.log("Client connected to Redis...");
                });
                                client.on("ready", (err) => {
                                console.log("Redis ready to use");
                });
                client.on("error", (err) => {
                console.error("Redis Client", err);
                });
                client.on("end", () => {
                console.log("Redis disconnected successfully");
                });
                await client.connect();
                return client;
}

async function storeOTP(userId, otp, duration) {
        try {
                const otpExpiredAt = new Date(new Date().setMinutes(new Date().getMinutes() + parseInt(duration)));

                const result = await query(
                        `UPDATE ${authTable} SET otp=?, expired_at=?, otp_used=false WHERE contact_id = ?`,
                        [otp, otpExpiredAt, userId]
                );
                const debugResult = await query(
                        `SELECT * from ${authTable} WHERE contact_id = ?`,
						[userId]
					
                );

                if (result && result.affectedRows) return true;

                return false;

        } catch (err) {
                console.log(err);
                return false;
        }
}


/* Redis */

const addToken = async(user_id, token, expiredAt) => {
        try {
                const client = await createRedisConnection();
                await client.SET(user_id, token); // Overwrite token if existed
                await client.EXPIREAT(user_id, expiredAt/1000); // sets the token expiration date based on the password expired datetime.
                return true;
        } catch (e) {
                console.error("Token not added to cache")
                console.log(e);
                return false;
    }
};

const getToken = async (userId) => {
    try {
        const client = await createRedisConnection();
        const token = await client.GET(userId); // get the token from the cache and return its value
        return token;
    } catch (e) {
        console.error("Fetching token from cache failed")
    }
};

const blacklistToken = async(key) => {
    try {
		const client = await createRedisConnection();
        await client.del(key);
        return;
    } catch(e) {
        console.error("Token not invalidated")
    }
};

async function increaseFailedAttempt(user) {
        const result = await query(
                `SELECT failed_attempt FROM auth WHERE contact_id = ?`,
                [user.contact_id]
        );

        const failedAttempts = result[0].failed_attempt + 1;
        await query(
                `UPDATE ${authTable} SET failed_attempt = ${failedAttempts} WHERE contact_id = ?`,
                [user.contact_id]
        );

        // If failed attempts > threshold, lock account
        if (failedAttempts > parseInt(process.env.FAILED_LOGIN_THRESHOLD)) {
                await query(
                        `UPDATE ${userTable} SET is_locked = true WHERE contact_id = ?`,
                        [user.contact_id]
                );

                const unlockAt = new Date(new Date().setMinutes(new Date().getMinutes() + parseInt(process.env.ACCOUNT_LOCK_DURATION_MIN)));

                await query(
                        `UPDATE ${userTable} SET unlock_at = ? WHERE contact_id = ?`,
                        [unlockAt, user.contact_id]
                );

				utils.sendLockAccountEmail(
					{
						dst: user.email,
						name: user.first_name,
						duration: process.env.ACCOUNT_LOCK_DURATION_MIN
					}
				);

				
        }

        return failedAttempts;
}

async function lockAccount(user) {
	await db.query(
		`UPDATE ${userTable} SET is_locked = true WHERE contact_id = ?`,
		[user.contact_id]
	)
}


module.exports = {
	query,
	storeOTP,
	getToken,
	addToken,
	blacklistToken,
	increaseFailedAttempt,
}
