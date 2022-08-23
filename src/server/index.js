const express = require('express');
const bodyParser = require('body-parser');
const execFile = require('child_process').execFile;

const app = express();
app.use(bodyParser.urlencoded({ extended: false }));
app.use(bodyParser.json());

app.use(function(req, res, next) {
    res.header("Access-Control-Allow-Origin", "*");   
    res.header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");   
    next(); 
});

app.get('/*', function (req, res) {
	res.send('Hello from adjustment calc server');
});

app.post('/calc', function(req, res) {
    const args = req.body.args;
    const execPath = path.join(__dirname, './C++/build/camCalibNode');
    console.log(execPath);
    const compute = execFile(execPath, args.split(" "), function (err, stdout, stderr) {
        if (err) {
            console.log('error');
            console.log(stderr);
            return res.status(400).json({
                error: 'Error launching adjustment calc program'
            });
        }
        if (stdout === '0') {
            console.log(stderr);
            return res.status(400).json({
                error: 'Error computing camera params'
            });
        }
        console.log(stdout);
        res.send(stdout);
    });
});

app.listen('3000', ()=> {
    console.log('Recalage server running on port 3000');
});
