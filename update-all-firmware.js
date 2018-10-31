var mdns = require('mdns-js');
var exec = require('child_process').exec;

var hardwareDescription = "md=Shed2,1";
var discoverDuration = 30;

var foundAddresses = [];

var browser = mdns.createBrowser("_hap._tcp");
browser.on('ready', function () {
    console.log("Starting discover for " + discoverDuration + " seconds");
    browser.discover(); 
});
 
browser.on('update', function (data) {
    if (!data.txt || data.txt[0] != hardwareDescription || !data.port)
        return;
    var address = data.addresses[0];

    if (!foundAddresses.includes(address))
    {
        foundAddresses.push(address);
    
        //console.log('data:', data);
        console.log( " * Device found at " + address);
    }
});

setTimeout(finishBrowsing, discoverDuration * 1000);

function finishBrowsing()
{
    browser.stop();
    if (foundAddresses.length == 0)
    {
        console.log("Sorry, no " + hardwareDescription + " found running on the local network");
        process.exit(1);
    }
    console.log("Found " + foundAddresses.length + " devices");

    foundAddresses.reduce(function(p, address) {
        return p.then(function(results) {
            
            return execPromise("make -C shed ota ESPIP=\"" + address + "\"").then(function(stdout) {
                //results.push(stdout);
                console.log(stdout);
                return results;
            });
        });
    }, Promise.resolve([])).then(function(results) {
        console.log('done, apparently');
        // all done here, all results in the results array
    }, function(err) {
        console.log(err);
        // error here
    });
}


///

var execPromise = function(cmd) {
    return new Promise(function(resolve, reject) {
        exec(cmd, function(err, stdout) {
            if (err) return reject(err);
            resolve(stdout);
        });
    });
}


