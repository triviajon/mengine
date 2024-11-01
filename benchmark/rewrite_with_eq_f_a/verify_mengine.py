import json
import os
import subprocess
import time

with open('mengine_benchmark_results.json') as f:
    mengine_results = json.load(f)

verification_results = {}

for key, entry in mengine_results.items():
    output = entry['output']
    
    with open('temp.v', 'w') as temp_file:
        temp_file.write(output)
    
    start_time = time.time()
    result = subprocess.run(['coqc', 'temp.v'], capture_output=True, text=True)
    elapsed_time = time.time() - start_time

    if 'Error' in result.stderr:
        verification_results[key] = {"status": "FAILED", "error": result.stderr.strip(), "time": elapsed_time}
    else:
        verification_results[key] = {"status": "PASSED", "time": elapsed_time}
    
    os.remove('temp.v')
    
    with open('verification_results.json', 'w') as f:
        json.dump(verification_results, f, indent=4)
