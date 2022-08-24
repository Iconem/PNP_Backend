const express = require('express');
const bodyParser = require('body-parser');
const morgan = require('morgan');
const controller = require('./controller');

// build server
const app = express();

// add json parsing of requests
app.use(bodyParser.urlencoded({ extended: false }));
app.use(bodyParser.json());

// add requests logging
app.use(morgan('combined'));

// add GET  /* route
app.get('/*', function (req, res) {
	res.send('Hello from PnP solver');
});

// add POST /solve route
app.post('/solve', controller.verifyBodyContent, controller.solve);


// start server
app.listen('3000', ()=> {
    console.log('PnP solver running on port 3000. Waiting for requests...');
});
