import subprocess
import os
import time

def run_experiment (workers, port):
    server_log_file = f'server_log_w{workers}.txt'
    client_log_file = f'client_log_w{workers}.txt'

    server_cmd = f'stdbuf -oL ./server_multi -q 1000 -w {workers} 2223'
    server_proc = subprocess.Popen(server_cmd.split(), stdout=open(server_log_file, 'w'), stderr=subprocess.STDOUT)

    time.sleep(2)

    client_cmd = f'../client -a 37 -s 20 -n 1500 -d 0 2223'
    with open(os.devnull, 'w') as devnull:
        subprocess.run(client_cmd.split(), stdout=devnull, stderr=subprocess.STDOUT)

    server_proc.terminate()

    print(f'Experiment with -w {workers} completed')

def automate_experiments ():
    for w in range (4, 9, 2):
        run_experiment(w, 2222)
        time.sleep(1)

if __name__ == "__main__":
    automate_experiments()