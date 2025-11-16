"use strict";

const http = require('http');

function testEndpoint(hostname, port, endpoint, verb, argdata, expectedStatusCode, expectedData, callback) {
    const options = {
        hostname: hostname,
        port: port,
        path: endpoint,
        method: verb,
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': data ? Buffer.byteLength(data) : 0
        }
    };

    const req = http.request(options, (res) => {
        let responseData = '';

        res.on('data', (chunk) => {
            responseData += chunk;
        });

        res.on('end', () => {
            if (res.statusCode === expectedStatusCode) {
                if(responseData === expectedData) {
                    console.log(`Test passed for ${verb} ${endpoint}`);
                    callback(null, responseData, argdata);
                } else {
                    console.error(`Test failed for ${verb} ${endpoint}: Expected data "${expectedData}", got "${responseData}"`);
                    callback(new Error(`Expected data "${expectedData}", but got "${responseData}"`), null, argdata);
                }
            } else {
                console.error(`Test failed for ${verb} ${endpoint}: Expected ${expectedStatusCode}, got ${res.statusCode}`);
                callback(new Error(`Expected status code ${expectedStatusCode}, but got ${res.statusCode}`), null, argdata);
            }
        });
    });

    req.on('error', (e) => {
        console.error(`Problem with request: ${e.message}`);
        callback(e, null, argdata);
    });

    if (data) {
        req.write(argdata);
    }
    req.end();
}

module.exports = {
    testEndpoint
};