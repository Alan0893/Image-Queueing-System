import pandas as pd
import os
import re

def log_all_server_stats_to_csv(directory):
    # Define the regex pattern to match files like 'server_log_a{a_value}.txt'
    file_pattern = re.compile(r'server_log_a(\d+)\.txt')

    # Initialize an empty list to store the results for all files
    results = []

    # Scan through all files in the specified directory
    for filename in os.listdir(directory):
        # Check if the filename matches the pattern
        match = file_pattern.match(filename)
        if match:
            # Extract the a_value from the filename
            a_value = int(match.group(1))  # Convert to integer for sorting

            # Process each matching log file and extract statistics
            file_path = os.path.join(directory, filename)
            column_names = [
                'RequestID',
                'ClientRequestTimestamp',
                'ClientRequestLength',
                'ReceiptTimestamp',
                'CompletionTimestamp'
            ]
            data = {col: [] for col in column_names}

            total_busy_time = 0.0
            previous_timestamp = None

            with open(file_path, 'r') as file:
                lines = file.readlines()

                for line in lines:
                    if line.startswith("R"):
                        parts = line.strip().split(',')
                        if len(parts) != 5:
                            continue  # Skip invalid lines
                        request_id_and_timestamp, length, timestamp2, start_timestamp, completion_timestamp = parts
                        request_id, client_request_timestamp = request_id_and_timestamp.split(':')

                        data['RequestID'].append(request_id.strip())
                        data['ClientRequestTimestamp'].append(float(client_request_timestamp.strip()))
                        data['ClientRequestLength'].append(float(length.strip()))
                        data['ReceiptTimestamp'].append(float(timestamp2.strip()))
                        data['CompletionTimestamp'].append(float(completion_timestamp.strip()))

                        # Calculate busy time for each request
                        busy_time = float(completion_timestamp.strip()) - float(start_timestamp.strip())
                        total_busy_time += busy_time

            df = pd.DataFrame(data)

            if not df.empty:
                total_time = df['CompletionTimestamp'].iloc[-1] - df['ClientRequestTimestamp'].iloc[0]
                queueing_time = df['ReceiptTimestamp'] - df['ClientRequestTimestamp']
                service_time = df['CompletionTimestamp'] - df['ReceiptTimestamp']
                df['ResponseTime'] = queueing_time + service_time
                average_response_time = df['ResponseTime'].mean()

                # Calculate server utilization
                utilization = total_busy_time / total_time if total_time > 0 else 0
            else:
                total_time = 0
                average_response_time = 0
                utilization = 0

            # Store the results for each log file
            results.append({
                'a_value': a_value,  # Store a_value for sorting
                'File': filename,
                'AverageResponseTime': average_response_time,
                'TotalBusyTime': total_busy_time,
                'TotalTime': total_time,
                'Utilization': utilization
            })

    # Convert the results to a DataFrame and sort by a_value
    results_df = pd.DataFrame(results).sort_values(by='a_value')

    # Write the results to a CSV file
    output_csv = 'server_log_summary.csv'
    results_df.to_csv(output_csv, index=False)

    print(f'Summary of all server logs has been written to {output_csv}.')

# Example usage:
log_all_server_stats_to_csv('../partC_d1')  # Use the current directory as the target folder
