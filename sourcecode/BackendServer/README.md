# Azure 
## Install dependencies
```bash
$ sudo apt update
 
$ curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash - &&\
$ sudo apt-get install -y nodejs mariadb-server
 
$ sudo apt install npm
$ sudo npm install pm2@latest -g
```

## Setup database
```bash
$ sudo systemctl enable mysql
$ sudo systemctl start mysql
$ sudo mysql 
```
Inside MySQL shell
```mysql
CREATE DATABASE [db_name];
CREATE USER  '[user]'@'%' IDENTIFIED BY '[password]';
GRANT ALL ON *.* to '[user]'@'%' IDENTIFIED BY '[password]' WITH GRANT OPTION;
FLUSH PRIVILEGES;

DROP TABLE IF EXISTS `[db_name]`.`auth`;
DROP TABLE IF EXISTS `[db_name]`.`calls`;
DROP TABLE IF EXISTS `[db_name]`.`contact`;

CREATE TABLE contact (contact_id UUID NOT NULL DEFAULT UUID(), email VARCHAR(64) NOT NULL, last_name VARCHAR(64) NOT NULL, first_name VARCHAR(64) NOT NULL, ip_address VARCHAR(15) NOT NULL, password CHAR(60) NOT NULL, password_expired_at DATETIME NOT NULL, is_active BOOL DEFAULT FALSE, is_locked BOOL DEFAULT FALSE, unlock_at DATETIME, UNIQUE (email), UNIQUE (contact_id), UNIQUE (ip_address));

CREATE TABLE auth (contact_id UUID NOT NULL, otp CHAR(6), expired_at DATETIME, otp_used BOOL DEFAULT FALSE, failed_attempt INTEGER NOT NULL DEFAULT 0, UNIQUE (contact_id), FOREIGN KEY(contact_id) REFERENCES contact(contact_id) ON DELETE CASCADE);

CREATE TABLE conversation (from_contact UUID NOT NULL, to_contact UUID NOT NULL, started_at DATETIME NOT NULL, stopped_at DATETIME, status INTEGER, FOREIGN KEY (from_contact) REFERENCES contact(contact_id) ON DELETE CASCADE, FOREIGN KEY (to_contact) REFERENCES contact(contact_id) ON DELETE CASCADE, UNIQUE `unique_index`(`from_contact`, `to_contact`, `started_at`));

CREATE TABLE token (contact_id UUID NOT NULL, token CHAR(32) NOT NULL, created_at DATETIME NOT NULL, expired BOOL DEFAULT FALSE, FOREIGN KEY (contact_id) REFERENCES contact(contact_id));
```

## Install node modules
```bash
$ cd lgvideochat/BackendServe && npm install
```

## Run server
### Source environment
Create a .env file with the following information
```
# Generate secret token inside nodejs shell
# require('crypto').randomBytes(64).toString('hex')

TOKEN_SECRET=459a9d4aab75c44abf917bdc12f3eb9371dcf034b763bd31a62f4d5a14f13f4ed49c8c3ffc3584ad2656f768fc9c2931a73d2ab247d70fe8fe71b831f9a65f8c
ACCESS_TOKEN_EXPIRED_DURATION=24
REFRESH_TOKEN_EXPIRED_DURATION=720
ACTIVATION_TOKEN_EXPIRED_DURATION=2

DATABASE_HOST=127.0.0.1
DATABASE_USER=lge
DATABASE_PASSWORD=lge@123
DATABASE_NAME=lgedb

BASE_URL=https://127.0.0.1:443

MAIL_PORT=587
MAIL_HOST=smtp.ethereal.email
MAIL_USER=chaim48@ethereal.email
MAIL_PASSWORD=eezpJY5ZAR8V9hRQbT

LOGIN_OTP_DURATION_MIN=10
RESET_PASSWORD_OTP_DURATION_MIN=60
UPDATE_DATA_OTP_DURATION_MIN=0
RESET_TOKEN_DURATION_HOUR=2
FAILED_LOGIN_THRESHOLD=5
ACCOUNT_LOCK_DURATION_MIN=30

```
### Local
```bash
$ node index.js
```
### Using pm2
```bash
$ pm2 start index.js
$ pm2 startup systemd 
```

### Ethereal Mail Service
Go to https://ethereal.email/messages and create a mail account

# Create self-signed certificate
https://www.nginx.com/blog/using-free-ssltls-certificates-from-lets-encrypt-with-nginx/
```bash
openssl genrsa -out lge-backend-key.pem 2048 
openssl req -new -sha256 -key lge-backend-key.pem -out lge-backend-csr.pem 
openssl x509 -req -in lge-backend-csr.pem -signkey lge-backend-key.pem -out lge-backend-cert.pem 

```
