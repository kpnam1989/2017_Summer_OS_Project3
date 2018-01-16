import time
import os
import sys
import argparse
import json
import datetime
from nelson.gtomscs import submit

def print_report(results):
    for result in results:
      if 'description' in result:
        print "Description:", result['description']
      if 'traceback' in result:
        print "Traceback:"
        print result['traceback']
      if 'output' in result:
        output = result['output']
        if 'client_returncode' in output:
          print "Client Returncode:", output['client_returncode']
        if 'server_returncode' in output:
          print "Server Returncode:", output['server_returncode']
        if 'server_console' in output:
          print "Server Console:"
          print output['server_console']
        if 'client_console' in output:
          print "Client Console:"
          print output['client_console']

def main():
  parser = argparse.ArgumentParser(description='Submits code to the Udacity site.')
  parser.add_argument('quiz', choices = ['proxy_server', 'proxy_cache', 'readme'])
  
  args = parser.parse_args()

  path_map = { 'proxy_server': 'server', 
               'proxy_cache': 'cache', 
               'readme': '.'}

  quiz_map = { 'proxy_server': 'pr3_proxy_server', 
               'proxy_cache': 'pr3_proxy_cache', 
               'readme' : 'pr3_readme'}

  files_map = { 'pr3_proxy_server': ['handle_with_curl.c', 'webproxy.c'],
                'pr3_proxy_cache':  ['handle_with_cache.c', 'shm_channel.h', 'webproxy.c', 'shm_channel.c', 'simplecached.c'],
            		'pr3_readme' : ['readme-student.md']}

  quiz = quiz_map[args.quiz]
  os.chdir(path_map[args.quiz])

  submit('cs8803-02', quiz, files_map[quiz])

if __name__ == '__main__':
  main()
