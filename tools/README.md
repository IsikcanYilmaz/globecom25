# automator.py
- Currently the main user interface to any test/experiment automation
- Usage: ./automator.py <sender serial port> <receiver serial port> [-r [router serial port(s)]]
    --rpl to enable rpl routing. If not, the router devices specified will be the route in that order (TODO)
    --experiment to enable the experiment
    --fitiot to note that we're working in the fitiot environment and comms with devices should be done accordingly. if this is enabled, the names of devices must be the fitiot names of the devices
    --results_dir to point where the output should go after the experiments
