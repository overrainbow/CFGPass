#ifndef UTIL_H
#define UTIL_H

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/StringRef.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llvm;
using namespace std;

// Get instructions from each block
// Return summary
bool getInstrMix(BasicBlock* bb, int* a) {
	// floating-point operation fadd, fmul, fdiv, fsub. index 0
	// memory operation read, write. index 1	
	// control operation, index 2
	// integer operation, index 3
	for (BasicBlock::iterator bi = bb -> begin(); bi != bb -> end(); ++bi) {
		Instruction &ins = *bi;
		//outs()<<ins.getOpcodeName()<<"\n";					

		// check vector instruction
		int vScale = 1;
		if (ins.getNumOperands() > 0)
		{
			Type* ty = ins.getOperand(0) -> getType();
			if (ty -> isVectorTy())
			{
				vScale = ty -> getVectorNumElements();
			}
		}

		switch(ins.getOpcode()) {
			default: break;
			case Instruction::FAdd:
			case Instruction::FMul:
			case Instruction::FDiv:
			case Instruction::FSub:
					 a[0] += vScale; break;	
			case Instruction::Store:
			case Instruction::Load:
			case Instruction::Fence:
			case Instruction::GetElementPtr:
					 a[1] += vScale; break;
			case Instruction::Ret:
			case Instruction::Br:
			case Instruction::Switch:
			case Instruction::Resume:
					 a[2] += vScale; break;	
			case Instruction::Add:
			case Instruction::Mul:
			case Instruction::UDiv:
			case Instruction::SDiv:
			case Instruction::Sub:
					 a[3] += vScale; break;	
		}

	}

	return true;
}

// get input file name from Function instance
string getBaseName(Function& F) {

	string baseOutName;

	DISubprogram* dis = F.getSubprogram();
	if (dis){
		string temp = dis->getFilename().str();
		baseOutName = temp.substr(temp.find_last_of("/")+1);
	} 

	return baseOutName;
}


// Function for write CFG info to file
// Each loop within the function generates an output
// Params:
//		li   - LoopInfo
//		BPI  - BranchProbability
//		F    - Function info 
//      mode - output mode
//			   0 : matrix only
//			   1 : dot file only
void writeToFile(Function& F, LoopInfo& li, const BranchProbabilityInfo* BPI, int mode=0) {

	string baseOutName = getBaseName(F);
	string funcName	   = F.getName().str();
	// hash map as lookup table for 
	// basic block and its id
	unordered_map <BlockAddress*, int> bbMap;
	
	for (LoopInfo::iterator lp = li.begin(); lp != li.end(); lp++)
	{

		Loop *loop = *lp;
		// find outermost loop
		if (loop -> getParentLoop() != NULL) continue;
								
		// output name for each loop
		string outFileName = baseOutName;

		DebugLoc sline = loop -> getStartLoc();
		string ll;
		if (sline)
		{
			ll = to_string(sline.getLine());
			outs() << "Loop starts at line: " << ll <<"\n";
			outFileName.append("."+ll);
			if (mode == 0) {
				outFileName.append(".matrix");
			} else {
				outFileName.append(".dot");
			}

			
		}	
		// outstream
		error_code RC;	
		raw_fd_ostream out(StringRef(outFileName), RC, sys::fs::OpenFlags::F_None);
		//raw_fd_ostream out(StringRef(outFileName), RC, sys::fs::F_None);
		
		if (mode == 1) {
			// write dot file head
			out << "digraph \"CFG for loop in function \'" << funcName << "\' at line " << ll << "\" {\n";
			out << "\tlabel = " << "\"CFG for loop in function \'" << funcName << "\' at line " << ll << "\";\n\n";
		}

		// label each block as id within the loop
		// start from 0
		int label = 0;
		for (Loop::block_iterator bi = loop -> block_begin(); bi != loop -> block_end(); bi++)
		{
			//(*bi)->printAsOperand(outs(),false);
			// create a mapping
			bbMap[BlockAddress::get(*bi)] = label++;
			
			// setName not working here 
			// since it doesn't allow duplicate name
			//(*bi) -> setName(Twine(label));
			//int pid;
			// ((*bi) -> getName()).getAsInteger(10, pid);
		}
		float* matrix;
		if (mode == 0) {
		   matrix = new float[label*label];
		   for (int i = 0; i < label; i++)
			   for (int j = 0; j < label; j++) {	
				   matrix[i*label + j] = 0.0f;
		   }
		 

		   // need to modify the number of children for loop header
		   // as 1 since the false branch of loop header is not included
		   // in the basic blocks, so the loop header branch is removed
		   // it means the branch probability from loop header to the first basic block (0->1) is 100%

		   matrix[0 * label + 1] = 1.0f;

		}
	
		// storage for instruction mix for each basic block
		vector<vector<int>> nodeInstr(label, vector<int>(4, 0));

		for (Loop::block_iterator bi = (loop -> block_begin()); bi != loop -> block_end(); bi++)
		{
			BasicBlock* bb = *bi;				
			BlockAddress* bAddr = BlockAddress::get(bb);
			// label(id) for current basic block
			int pid = bbMap[bAddr];
			int mix[4] = {0};
			getInstrMix(bb, mix); 
			if (mode == 1) {
				// write node info
				//out << "\t" << (bb->getName()).str() <<" [shape=record,label=\"{";
				out << "\t" << to_string(pid) <<" [shape=record,label=\"{";
				out << to_string(pid) << ":\\l FLOP: " << mix[0] << "\\l IntOp: " << mix[3] <<"\\l MemOp: " << mix[1] << "\\l CtrlOp: " << mix[2] << "\\l";
			}
			const TerminatorInst *tInst = bb -> getTerminator();
			unsigned numSucc = tInst -> getNumSuccessors();
			
			//int pid;
			// parent (current) bb id 
			//((*bi) -> getName()).getAsInteger(10, pid);

			// get instruction mix for node
			for (int i = 0; i < 4; i++) {
				nodeInstr[pid][0] = mix[0];
				nodeInstr[pid][1] = mix[1];
				nodeInstr[pid][2] = mix[2];
				nodeInstr[pid][3] = mix[3];
			}	

			if (numSucc == 0) 
			{
				if (mode == 1) {
					// no child, close node info here
					out << "}\"];\n";
				}	
			}

			if (numSucc > 0)
			{

				if (numSucc == 1)
				{
					// has only one child
					// child bb id
					int cid = bbMap[BlockAddress::get(tInst->getSuccessor(0))];
					// (tInst -> getSuccessor(0) -> getName()).getAsInteger(10, cid);
		
					if (mode == 1) {
						// close node info, add edge info
						out << "}\"];\n";
						out << "\t" << to_string(pid) << " -> " << to_string(cid);
						// edge weight is 1.00
						out <<" [label = \"1.00\"];\n";
					} else {
						matrix[pid * label + cid] = 1.0f;
					}
	
				} else {
					// process branch	
					// assume two branch at most for now
					//int cid;
					
					if (mode == 1) {
						// close node info
						out <<"|{<s0>T|<s1>F}}\"];\n";
					}

					for (unsigned i = 0; i < numSucc; i++) {
						BasicBlock *succBB = tInst->getSuccessor(i);
						BranchProbability bp = BPI->getEdgeProbability(bb, succBB);	
						//succBB->printAsOperand(outs(),false);
						float value = rint((float)bp.getNumerator()/bp.getDenominator()*100)/100;
						//(succBB -> getName()).getAsInteger(10, cid);
						int cid = bbMap[BlockAddress::get(succBB)];
		
						if (mode == 0) {
							matrix[pid * label + cid] = value;
						} else {
							// write edge
							// for the loop head we need to create a fake node 
							// to represent the node outside loop to complete branch
							string succName = to_string(cid);
							if (succName.empty())
							{
								out << "\t" << to_string(pid) << ":s" << i << " -> outside";
								out << " [label =\"" << value << "\"];\n";
								// create an outside node
								out << "\toutside [shape=record,label=\"outside loop\"];\n";

							} else {
								out << "\t" << to_string(pid) << ":s" << i << " -> " << to_string(cid);
								out << " [label =\"" << value << "\"];\n";
							}
						}	
					}

				}	

			}	
		}

		if (mode == 0) {
		// write matrix	
		// first line of the output file is the matrix size
			out << label <<"\n";
			for (int i = 0; i < label; i++) {

				// write instruction mix for the node
				// in the beginning of the line
				out << nodeInstr[i][0] << ":";
				out << nodeInstr[i][1] << ":";
				out << nodeInstr[i][2] << ":";
				out << nodeInstr[i][3] << "\t";
							
				for (int j = 0; j < label; j++) {	
					out << matrix[i*label + j] << "\t";
				}	
				out <<"\n";
			}

			delete[] matrix;
		
		} else {
			out<<"}";
		}

		// close stream
		out.flush();
		out.close();

	}// end for 

}



#endif
