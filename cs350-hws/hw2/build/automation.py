import subprocess
import os
import time

def run_experiment(a_value, server_port=2222, num_requests=1000):
    # File names to save logs for each experiment
    server_log_file = f'server_log_a{a_value}.txt'
    client_log_file = f'client_log_a{a_value}.txt'

    # Start the server
    server_cmd = f"./server_q {server_port}"
    server_proc = subprocess.Popen(server_cmd.split(), stdout=open(server_log_file, 'w'), stderr=subprocess.STDOUT)
    
    # Give server time to start properly
    time.sleep(2)
    
    # Run the client
    client_cmd = f"../client -a {a_value} -s 15 -n {num_requests} {server_port}"
    with open(client_log_file, 'w') as client_log:
        subprocess.run(client_cmd.split(), stdout=client_log, stderr=subprocess.STDOUT)
    
    # Wait for the client to finish, then kill the server
    server_proc.terminate()
    
    print(f"Experiment with -a {a_value} completed.")

def automate_experiments(num_experiments=15, server_port=2222, num_requests=1000):
    for a_value in range(1, num_experiments + 1):
        run_experiment(a_value, server_port, num_requests)
        # Optional sleep to ensure clean separation between experiments
        time.sleep(1)

if __name__ == "__main__":
    # Run 15 experiments, varying the `-a` parameter from 1 to 15
    automate_experiments()
