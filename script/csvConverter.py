'''
	Helper script to convert plain test generated
	by llvm to csv format
'''
import sys
import re
import os
import numpy as np
import csv

def convert(fileName, outPath):
	mat_size = 0
	out_mat = []
	# read file
	with open(fileName) as f:
		# first line flag
		fl_flag = False
		for line in f:
			if not fl_flag:
				# first line
				mat_size = int(line)
				fl_flag = True
			else:
				line_value = re.split(r'\t+', line.rstrip())
				# instruction counters
				row = re.split(r':+', line_value[0])
				row = map(int, row)
				row.extend(map(float,line_value[1:]))			
				out_mat.append(row)
	#return np.array(adj_prob), np.array(instr), mat_size
	
	# write file
	with open(outPath, 'wb') as csvfile:
		csvwriter = csv.writer(csvfile, delimiter=',')
		csvwriter.writerows(out_mat)


if __name__ == "__main__":
	filePath = sys.argv[1]
	# replace suffix of path with .csv
	outPath = os.path.splitext(filePath)[0] + '.csv'
	convert(filePath, outPath)	
