const dotenv = require('dotenv');

dotenv.config();

const db_host           = process.env.DATABASE_HOST;
const db_user           = process.env.DATABASE_USER;
const db_password       = process.env.DATABASE_PASSWORD;
const db_name           = process.env.DATABASE_NAME;

const config = {
        db: {
                host: db_host,
                user: db_user,
                password: db_password,
                database: db_name,
                multipleStatements: false,
        },
        listPerPage: 10,
};

module.exports = config;
