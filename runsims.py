import subprocess
compressed_traces = [
"art-100M.trace-ld-st.txt.gz",\
"gcc-10M.trace-ld-st.txt.gz",\
"gcc-50M.trace-ld-st.txt.gz",\
"go-100M.trace-ld-st.txt.gz",\
"hmmer-100M.trace-ld-st.txt.gz",\
"libquantum-100M.trace-ld-st.txt.gz",\
"mcf-100M.trace-ld-st.txt.gz",\
"sjeng-100M.trace-ld-st.txt.gz",\
"sphinx3-100M.trace-ld-st.txt.gz"]

configs = [" 4 2048 8 100000 ",\
           " 4 4096 8 100000 ",\
           " 4 8192 8 100000 "]

en_configs = ["Configuration A: 64 Ki, 4-way, 8-byte blocks, 2048 sets",\
              "Configuration B: 64 Ki, 4-way, 8-byte blocks, 4096 sets",\
              "Configuration C: 64 Ki, 4-way, 8-byte blocks, 8192 sets"]
caches = ["standard", "robinhood", "zcache"]

i = 0;
for config in configs:
    print "===============" + en_configs[i] + "==============="
    for cache in caches:
        print cache
        for trace in compressed_traces:
            command = "./cache " + cache + config + "Project_3/" + trace
            output = subprocess.check_output(command, shell=True)
            print trace 
            print output
        print
    i = i + 1


