import subprocess
import os
import time

def run_experiment(arr_rate, policy, port=2222):
    server_log_file = f'server_log_{policy}_a{arr_rate}.txt'
    client_log_file = f'client_log_{policy}_a{arr_rate}.txt'

    # Start the server with the specified policy (FIFO or SJN)
    server_cmd = f'stdbuf -oL ./server_pol -w 2 -q 100 -p {policy} {port}'
    server_proc = subprocess.Popen(server_cmd.split(), stdout=open(server_log_file, 'w'), stderr=subprocess.STDOUT)

    # Allow server to start
    time.sleep(2)

    # Run the client
    client_cmd = f'../client -a {arr_rate} -s 20 -n 1500 {port} > /dev/null'
    with open(client_log_file, 'w') as client_log:
        subprocess.run(client_cmd.split(), stdout=client_log, stderr=subprocess.STDOUT)

    # Stop the server
    server_proc.terminate()

    print(f'Experiment with policy {policy} and arr_rate {arr_rate} completed')

def automate_experiments():
    policies = ['FIFO', 'SJN']  # List of policies
    for policy in policies:
        for arr_rate in range(22, 41, 2):
            run_experiment(arr_rate, policy)
            time.sleep(1)  # Small pause between experiments

if __name__ == "__main__":
    automate_experiments()
