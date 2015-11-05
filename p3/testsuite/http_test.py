import httplib
import urllib2
import sys
import threading
import time
from Queue import Queue

successes = Queue()
failures = Queue()

def parse_trace(tracefile='1.trace'):
    
    config = {}
    trace = []
    traces = [] #list of lists/dicts
    f = open(tracefile)

    section = None

    for l in f:
        l = l.strip()

        if len(l) == 0: continue
        
        #check for comments
        if l.startswith('%'):
            continue

        #check for sections
        if l.startswith('[') and l.endswith(']'):
            section = l[1:-1]
            if len(trace) != 0:
                traces.append(trace)
            trace = []
            continue

        #parse configuration
        if section == 'configuration':
            k,v = l.split('=')
            config[k] = v
        #parse trace sections
        elif section.startswith('trace'):
            #TODO have multiple trace
            trace.append(l)

    if len(trace) != 0:
        traces.append(trace)
            

    return config, traces           
 
def run_trace(config, traces, host, port):
    
    num_threads = int(config['threads'])
    threads = []
    for i in range(num_threads):
        if config['type'] == 'correctness':
            t =threading.Thread(target=run_correctness_trace,\
            args=(config,traces[i%len(traces)],host,port,i))
            threads.append(t)
            t.start()
        else:
            t = threading.Thread(target=run_performance_trace,\
            args=(config,traces[i%len(traces)],host,port,i))
            threads.append(t)
            t.start()

    for t in threads:
        t.join()

    analyze_trace()

def run_correctness_trace(config, trace, host, port, _id=0):
    
    num_requests = int(config['requests'])
    for i in range(num_requests):
        
        for req in trace:
            if ' ' in req:
                req, assertion = req.split(' ', 1)
            else:
                assertion = None
                #have http_request return a value
            http_request(host, port, req, float(config['sleeptime']), _id=_id,\
            assertion=assertion)
def run_performance_trace(config, trace, host, port, _id=0):
    
    num_requests = int(config['requests'])
    for i in range(num_requests):

        for req in trace:
            http_request(host, port, req, float(config['sleeptime']), _id=_id)

def http_request(host, port, obj, sleeptime=0.0, method='GET', **kwargs):

    _id = 0
    if '_id' in kwargs:
        _id = kwargs['_id']
    
    assertion = None
    if 'assertion' in kwargs:
        assertion = kwargs['assertion']

    start = time.time()
    try:
        conn = httplib.HTTPConnection(host, port)
        time.sleep(sleeptime)
        conn.request(method, obj)

        response = conn.getresponse()
        responseStr = response.read()        
        conn.close()

        responseStr = responseStr.strip()
        
    except Exception as e:
        #HTTP Connection error
        #print 'HTTP Connection Error', e
        conn.close()
        failures.put(start)
        return False
    
    end = time.time()

    if assertion is not None and responseStr != assertion:
        print 'Incorrect Response: Correct=%s\tYours=%s'%(assertion,\
        responseStr)
        failures.put(start)
        return False

        
    if response.status != 200:
        #failed request
        print obj, 'Failed Request with incorrect status: %s' % response.status
        failures.put(start)
        return False

    #shoudl we hash for correctness?
    #print _id, 'Successful Request', '%s:%s%s'%(host,port,obj),\
    #'length=%s'%len(responseStr), (end-start-sleeptime)*1000

    successes.put((start, (end-start-sleeptime)*1000))
    
    return True

def analyze_trace():
    
    suc = []
    fail = []

    try:
        while True:
            start, dur = successes.get(0)
            suc.append((start, dur))
    except:
        pass

    try:
        while True:
            start = failures.get(0)
            fail.append(start)
    except:
        pass

    total = len(suc) + len(fail)
    
    print 'Total: %s'%total, 'Success: %s'%len(suc), 'Fail: %s'%len(fail)

    suc.sort(key=lambda x: x[0])
    
    total_dur = 0.0
    for start,dur in suc:
        total_dur += dur
    avg_resp_time = total_dur/float(len(suc))

    print 'Average Response Time: %s ms' % avg_resp_time

    total_time = (suc[-1][0] - suc[0][0])
    print 'Total Time = %s seconds' % total_time
    


if __name__ == '__main__':

    if len(sys.argv) != 4:
        print 'Not enough arguments'
        print 'usage python http_test.py <host> <port> <trace file>'
        exit()

    host = sys.argv[1]
    port = sys.argv[2]
    tracefile = sys.argv[3]



    config, traces = parse_trace(tracefile)
    
    print 'Running Trace: %s' % tracefile
    run_trace(config, traces, host, port)
    


