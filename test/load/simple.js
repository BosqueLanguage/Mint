"use strict";

const runner = require('../lrunner.js');

const ops = [
    {
        endpoint: '/hello',
        verb: 'get',
        argdata: null,
        expectedStatusCode: 200,
        expectedData: '{"message": "Hello, world!"}'
    },
    {
        endpoint: '/fib',
        verb: 'get',
        argdata: '{"value": 10}',
        expectedStatusCode: 200,
        expectedData: '{"value": 55}'
    },
    {
        endpoint: '/helloname',
        verb: 'get',
        argdata: '{"name": "bob"}',
        expectedStatusCode: 200,
        expectedData: '{"message": "Hello, bob!"}'
    },
    {
        endpoint: '/fib',
        verb: 'get',
        argdata: '{"value": 20}',
        expectedStatusCode: 200,
        expectedData: '{"value": 6765}'
    },
    {
        endpoint: '/sample.json',
        verb: 'get',
        argdata: null,
        expectedStatusCode: 200,
        expectedData: null
    }
];

//time curl -X get -d '{"value": 45}' http://localhost:8000/fib

let completed = 0;
let errors = 0;

function runLoad() {
    const endpoints = ops.map(op => op.endpoint);
    const timings = runner.createTimingInfo(endpoints);

    let mops = [];
    for (let i = 0; i < 10; i++) {
        mops = mops.concat(ops);
    }
    const total = mops.length;
    runner.runActions('localhost', 8000, 10, timings, mops, (err, results) => {
        completed++;
        if (err) {
            errors++;
            console.error(`Run ${completed} completed with errors: ${err.message}`);
        } else {
            console.log(`Run ${completed} completed successfully.`);
        }

        if (completed === total) {
            console.log(`Load test completed: ${completed} runs, ${errors} errors.`);

            runner.printTimings(timings);
        }
    });
}

runLoad();
