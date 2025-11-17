"use strict";

const http = require('http');

/**
 * 
 * @param {string[]} endpoints 
 * @returns {Object<string, {totalTime: number, lastring: number[], lrpos: number, count: number}>}
 */
function createTimingInfo(endpoints) {
    let timingInfo = {};
    for(let i = 0; i < endpoints.length; i++) {
        timingInfo[endpoints[i]] = {
            totalTime: 0,
            lastring: [],
            lrpos: 0,
            count: 0
        };
    }
    return timingInfo;
}

/**
 * @param {Object<string, {totalTime: number, lastring: number[], lrpos: number, count: number}>} timingInfo 
 */
function printTimings(timingInfo) {
    for(const endpoint in timingInfo) {
        const info = timingInfo[endpoint];

        console.log(`Endpoint: ${endpoint}`);
        console.log(`  Total Requests: ${info.count}`);

        if(info.count !== 0) {
            const avgTime = info.count > 0 ? (info.totalTime / info.count) : 0;

            const pct10 = Math.min(10, Math.floor(info.lastring.length * 0.9));
            const slows = info.lastring.sort((a, b) => b - a)[info.lastring.length - pct10];

            console.log(`  Average Time: ${avgTime.toFixed(2)} ms`);
            console.log(`  P90: ${slows} ms`);
        }
    }
}

/**
 * 
 * @param {Object<string, {totalTime: number, lastring: number[], lrpos: number, count: number}>} timingInfo 
 * @param {string} endpoint 
 * @param {number} startTime 
 * @param {number} endTime 
 */
function recordTiming(timingInfo, endpoint, startTime, endTime) {
    const duration = endTime - startTime;
    const info = timingInfo[endpoint];
    info.totalTime += duration;
    info.count += 1;

    // Maintain a list of the 1000 most recent times
    if(info.lastring.length < 1000) {
        info.lastring.push(duration);
    }
    else {
        info.lastring[info.lrpos] = duration;
        info.lrpos = (info.lrpos + 1) % 1000;
    }
}

/**
 * Test an HTTP endpoint with the specified parameters.
 * @param {string} hostname - The hostname of the server.
 * @param {number} port - The port number of the server.
 * @param {string} endpoint - The endpoint path to test.
 * @param {string} verb - The HTTP verb (get, post, etc.).
 * @param {string | null} argdata - The data to send with the request.
 * @param {number} expectedStatusCode - The expected HTTP status code.
 * @param {string | null} expectedData - The expected response data.
 * @param {Object<string, {totalTime: number, slow10: number[], count: number}>} timings - Timing information object.
 * @param {function} callback - The callback function to execute after the test.
 */
function runAction(hostname, port, endpoint, verb, argdata, expectedStatusCode, expectedData, timings, callback) {
    const options = {
        hostname: hostname,
        port: port,
        path: endpoint,
        method: verb,
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': argdata ? Buffer.byteLength(argdata) : 0
        }
    };

    const startTime = Date.now();
    const req = http.request(options, (res) => {
        let responseData = '';

        res.on('data', (chunk) => {
            responseData += chunk;
        });

        res.on('end', () => {
            const endTime = Date.now();
            recordTiming(timings, endpoint, startTime, endTime);

            if (res.statusCode === expectedStatusCode) {
                if(expectedData === null || responseData === expectedData) {
                    console.log(`Test passed for ${verb} ${endpoint}`);
                    callback(null, responseData);
                } else {
                    console.error(`Test failed for ${verb} ${endpoint}: Expected data "${expectedData}", got "${responseData}"`);
                    callback(new Error(`Expected data "${expectedData}", but got "${responseData}"`), null);
                }
            } else {
                console.error(`Test failed for ${verb} ${endpoint}: Expected ${expectedStatusCode}, got ${res.statusCode}`);
                callback(new Error(`Expected status code ${expectedStatusCode}, but got ${res.statusCode}`), null);
            }
        });
    });

    req.on('error', (e) => {
        console.error(`Problem with request: ${e.message}`);
        callback(e, null);
    });

    if (argdata) {
        req.write(argdata);
    }
    req.end();
}

/**
 * Run action tests against the specified endpoint.
 * @param {string} hostname 
 * @param {number} port 
 * @param {number | undefined} rate
 * @param {Object<string, {totalTime: number, slow10: number[], count: number}>} timings - Timing information object.
 * @param {{endpoint: string, verb: string, argdata: string | null, expectedStatusCode: number, expectedData: string | null}[]} infos 
 * @param {function} callback 
 */
function runActions(hostname, port, rate, timings, infos, callback) {
    if(rate === undefined) {
        for(let i = 0; i < infos.length; i++) {
            const info = infos[i];
            runAction(hostname, port, info.endpoint, info.verb, info.argdata, info.expectedStatusCode, info.expectedData, timings, callback);
        }
    }
    else {
        if(infos.length === 0) {
            return;
        }
        else {
            setTimeout(() => {
                const info = infos.shift();
                runAction(hostname, port, info.endpoint, info.verb, info.argdata, info.expectedStatusCode, info.expectedData, timings, callback);
                runActions(hostname, port, rate, timings, infos, callback);
            }, 1000 / rate);
        }
    }
}

module.exports = {
    createTimingInfo, printTimings,
    runActions
};
