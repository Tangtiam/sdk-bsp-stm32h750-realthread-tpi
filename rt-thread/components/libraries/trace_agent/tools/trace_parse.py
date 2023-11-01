import os
import sys
import struct
import json
import getopt

desc = {}
tid_name_map = {}
pid_name_map = {}

def load_desc(fn):
    global desc
    f = open(fn, 'r')
    data = f.read()
    items = json.loads(data)
    for item in items:
        desc[item['hash']] = item

def desc_find_item(hashkey):
    global desc
    if hashkey in desc:
        item = desc[hashkey]
        return item

    return None

def generate_item(desc_item, data):
    d = {}
    for item in desc_item['object']:
        if item['type'] == 'object':
            obj_data = data[item['offset'] : ]
            d[item['name']] = generate_item(item, obj_data)
            continue
        elif item['type'] == 'integer':
            if item['size'] == 1:
                fmt = 'B'
            elif item['size'] == 2:
                fmt = 'H'
            elif item['size'] == 4:
                fmt = 'I' # L is 8 byte width in 64bit Linux host. It should be 'I'.
        elif item['type'] == 'char':
            size = item['size']
            fmt = ('%ds' % size)
        elif item['type'] == 'number':
            if item['size'] == 4:
                fmt = 'f'
            if item['size'] == 8:
                fmt = 'd'
        data_item = data[item['offset'] : item['offset'] + item['size']]
        try:
            d[item['name']] = struct.unpack(fmt, data_item)[0]
            if item['type'] == 'char':
                d[item['name']] = d[item['name']].decode('ascii').rstrip('\x00')
        except Exception as e:
            print("unpack failed[%s]: data len - %d" % (fmt, len(data)))
            print(item)

    return d

def generate(input_fn, output_fn):
    trace_items = []

    # read all item in trace.bin
    f = open(input_fn, 'rb')
    while True:
        data = f.read(4)
        if len(data) != 4:
            break

        (hash, length) = struct.unpack('HH', data)
        # read next
        desc_item = desc_find_item(hash)
        if desc_item == None:
            print('bad trace file')
            print("position: %d data - 0x%04x%04x" % (f.tell() - 4, hash, length))
            # exit(-1)
            continue
        item = generate_item(desc_item, f.read(length))

        # add to name map
        if item.get('ph', 'NONE') == 'M':
            if 'tid' in item:
                tid_name_map[item['tid']] = item
            elif 'pid' in item:
                pid_name_map[item['pid']] = item

        # add thread name
        if item.get('name', 'NONE_NAME') == ' ':
            if item['tid'] in tid_name_map:
                item['name'] = tid_name_map[item['tid']]['args']['name']
        trace_items.append(item)

    # generate json data file
    trace_file = open(output_fn, 'w')
    trace_file.write(json.dumps(trace_items, indent=4))
    trace_file.close()

    # for item in trace_items:
    #    print(item)

def Usage():
    print('Usage: trace_parser -i input.bin -o trace.json')
    exit(0)

if __name__ == '__main__':
    print('Trace Parser')

    input_fn = 'trace.bin'
    output_fn = 'trace.json'

    try:
        opts,args = getopt.getopt(sys.argv[1:], "hi:o:", ["help", "input", "output"])
    except Exception as e:
        print(e)
        Usage()

    load_desc('desc.json')

    for opt,arg in opts:
        if opt in ('-h'):
            Usage()
            exit(0)
        elif opt in ('-i'):
            input_fn = arg
        elif opt in ('-o'):
            output_fn = arg

    generate(input_fn, output_fn)
