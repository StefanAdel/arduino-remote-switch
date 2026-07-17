export {}

// iobroker object IDs
const shouldVal   = [
                    "mqtt.0.arduino.irrigation.should.1",   // Seitenstreifen Ost (Garageneinfahrt)
                    "mqtt.0.arduino.irrigation.should.2",   // Garten hinten (Ost Seite / Spielturm)
                    "mqtt.0.arduino.irrigation.should.3",   // Garten Nord
                    "mqtt.0.arduino.irrigation.should.4",   // Garten West
                    "mqtt.0.arduino.irrigation.should.5"    // Garten hinten (West Seite)
                    ]                   
const isVal   = [
                    "mqtt.0.arduino.irrigation.is.1",
                    "mqtt.0.arduino.irrigation.is.2",
                    "mqtt.0.arduino.irrigation.is.3",
                    "mqtt.0.arduino.irrigation.is.4",
                    "mqtt.0.arduino.irrigation.is.5"
                    ]    
const indexEast     = 0;
const indexSouthE   = 1;
const indexNorth    = 2;
const indexWest     = 3;
const indexSouthW   = 4;

const duration_3min  =  3*60*1000;       
const duration_5min  =  5*60*1000;                
const duration_7min  =  7*60*1000;
const duration_8min  =  8*60*1000;
const duration_10min = 10*60*1000;

const AMOUNT_VALVE = 5;

const shouldPump =    "mqtt.0.arduino.irrigation.should.6";
const isPump =        "mqtt.0.arduino.irrigation.is.6";


const pause1day = "0_userdata.0.IrrigationControl.Pause1Day";
const pauseperm = "0_userdata.0.IrrigationControl.PausePermanently";

const pauseseeding = "0_userdata.0.IrrigationControl.PauseSeeding";


StopIrrigation();   // stop first in case iobroker broke down while irrigating and is now restarting the script.


schedule({hour: 0, minute:  0}, function() {
    StartIrrigation(indexEast, duration_7min/2, duration_8min, 6)
        .then((value) => { StartIrrigation(indexSouthE, duration_7min/2, duration_8min, 6)
            .then((value) => { StartIrrigation(indexSouthW, duration_3min/2, duration_8min, 14); }) });

});


// handling of pause-1-day => Reset at 8 in the morning as the pump runs regulary during night.
schedule({hour: 8, minute: 0}, function() {
     let val = false;

     if (getState(pause1day).val > 0) {
        console.info("IrrigationControl: Resetting pause-1-day variable to zero.");
        setState(pause1day, 0);
     }
});


/**
 * index:       The index of the relay to switch on
 * duration:    The duration to keep the relay switched on (in [ms])
 * recover:     The duration to wait for recover after switching off (in [ms])
 * cycles:      The amount of cycles / loops this should be done.
 */
async function StartIrrigation(index: number, duration: number, recover: number, cycles: number) {
    console.info("IrrigationControl: StartIrrigation (Index: " + index + ", Duration[s]: " + (duration/1000) + ", recover[s]: "
        + (recover/1000) + ", Cycles: " + cycles + ") called.");
        

    if (getState(pause1day).val > 0) {
        console.warn("IrrigationControl: Pause-1-day is set to true, skipping start-request for circuit: " + (index+1));
        return;
    }
    else if (getState(pauseperm).val > 0) {
        console.warn("IrrigationControl: Pause-Permanently is set to true, skipping start-request for circuit: " + (index+1));
        return;
    }


    if (index < 0 || index >= AMOUNT_VALVE) {
        console.error("IrrigationControl: Starting circuit " + (index+1) + " ist not known / out of bounds.");
        return
    }

    // start valve
    if (await SetCheckWaitValue(shouldVal[index], isVal[index], 0))
        console.info("IrrigationControl: Starting valve " + (index+1) + " successful.");

    for (let currC = 1; currC <= cycles; currC++) {
        console.debug("IrrigationControl: Starting circuit " + (index+1) + " (" + shouldVal[index] + "), cycle " + currC);

        if (await SetCheckWaitValue(shouldPump, isPump, 0))
            console.info("IrrigationControl: Starting pump successful (cycle: " + currC + ").");

        await wait(duration);

        if (await SetCheckWaitValue(shouldPump, isPump, 1))
            console.info("IrrigationControl: Stopping pump successful (cycle: " + currC + ").");

        await wait(recover);
    }

    await StopIrrigation(); // stop whatever hasn't been stopped already (for example the valve)

    console.info("IrrigationControl: StartIrrigation leaving function for circuit: " + (index+1));
}


async function StopIrrigation() {
    console.debug("IrrigationControl: Stopping all open circuits / valves");
    
    if (await SetCheckWaitValue(shouldPump, isPump, 1))
        console.info("IrrigationControl: Stopping pump successful.");

    // just for more resilience. Normally only one vale should be opened anyway.
    for (let i = 0; i < AMOUNT_VALVE; i++) {   
        if (getState(isVal[i]).val != 1) {  // seems to be open... 
            if (await SetCheckWaitValue(shouldVal[i], isVal[i], 1)) { // try to close
                console.info("IrrigationControl: Stopping circuit / valve " + (i+1) + " successful.");
            }
        }
    }

    console.debug("IrrigationControl: StopIrrigation leaving function");
}


async function SetCheckWaitValue(setidstr: string, getidstr: string,  expVal: number) : Promise<boolean> {
    let cnt = 0;
    let val = 0;
    
    setState(setidstr, expVal);

    do {
        await wait(1000);

        val = getState(getidstr).val;
        console.debug("IrrigationControl: Waiting for " + getidstr + " to become " + expVal);

        if (++cnt == 3600) {
            // 60 minutes gone.
            console.error("IrrigationControl: Waiting for " + getidstr + " to become " + expVal + "failed for 60 min. Aborting");
            return false;
        }
        else if (cnt % 100 == 0) {
            console.warn("IrrigationControl: Waiting for " + getidstr + " to become " + expVal + "failed for 100 secs. Starting Retry #" + (Number)(cnt/10));
            setState(setidstr, expVal);
        }
    } while (val != expVal);

    console.debug("IrrigationControl: Waiting for " + getidstr + " to become " + expVal + " successful.");
    return true;
}


