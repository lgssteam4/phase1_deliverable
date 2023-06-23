const express = require("express");
const dotenv = require("dotenv");
const https = require("https");
const util = require("util");
const fs = require("fs");
const path = require("path");
const userRouter = require("./routes/user");
const authRouter = require("./routes/auth");

const log_file = fs.createWriteStream(__dirname + '/logs/service.log', {flags : 'w'});
const log_stdout = process.stdout;

console.log = function(d) { 
  log_file.write(util.format(d) + '\n');
  log_stdout.write(util.format(d) + '\n');
};

const app = express();
const port = process.env.PORT || 3000;


app.use(express.json());

app.use(
  express.urlencoded({
    extended: true,
  })
);

app.use("/api/auth", authRouter);
app.use("/api/user", userRouter);

/* Error handler middleware */
app.use((err, req, res, next) => {
  const statusCode = err.statusCode || 500;
  console.error(err.message);
  console.error(err.stack);
  res.status(statusCode).json({ message: err.message });
  return;
});

app.get("/", (req, res) => {
	console.log("HERERERER");
  res.json({ message: "Welcome to LGE Chat Backend Server" });
});

const httpsServer = https.createServer(
	{
	key: fs.readFileSync(path.join(__dirname,
	    "certs", "lge-backend-key.pem")),
	cert: fs.readFileSync(path.join(__dirname,
	    "certs", "lge-backend-cert.pem")),
	},
	app 
)

httpsServer.listen(443, () => {
    console.log("HTTPS server up and running on port 443")
})
