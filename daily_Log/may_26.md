- UBC IT replied.
    - Not very helpful.
    - Will try log-in from a different location. There is a slight chance to be connection issue.

- Look into actual coding.
    - Code can't be tested yet, but I am looking into detailed pseudo/real code based on the documentations and example codes.
    - Will send code files to Lakshmanan for examination/testing

- Cloudlab extended by a week.


- Connected to phone via LAN



- Start to design root access experiments.

Experiment:
Goal: Sanity check with KSGL

Method:
- turn on perf counter measurement
- run graphic heavy workload

- Code
        while true; do
            cat /sys/class/kgsl/kgsl-3d0/gpu_busy_percentage
            cat /sys/class/kgsl/kgsl-3d0/gpubusy
            cat /sys/class/kgsl/kgsl-3d0/gpuclk
            echo "---"
            sleep 0.5
        done


Keep this device-info output in your daily log.
2. Build/deploy Mesa PPS.
3. Run pps-producer as root.
4. Save the exact terminal output.
5. If PPS fails, investigate KGSL perfcounter as fallback path.




Next Step:
- build Mesa PPS (first look for code examples, run sanity check on it)
- ask Lakshmanan to check code and for permission
- Deploy and run Mesa PPS.


