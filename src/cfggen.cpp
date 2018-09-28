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
#include "llvm/ADT/SCCIterator.h"
#include <unordered_set>
#include "util.h"

using namespace llvm;
using namespace std;

// inline functions

namespace {
	struct CfgPass : public FunctionPass {
		static char ID;
		CfgPass() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const
	{
		//AU.addRequired<DependenceAnalysisWrapperPass>();
		AU.addRequired<LoopAccessLegacyAnalysis>();
		AU.addRequired<LoopInfoWrapperPass>();
		AU.addRequired<MemoryDependenceWrapperPass>();
		AU.addRequired<DependenceAnalysisWrapperPass>();
		AU.addRequired<BlockFrequencyInfoWrapperPass>();
		AU.setPreservesAll();
	}
	
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

    virtual bool runOnFunction(Function &F) {

		LoopInfo &li = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
		MemoryDependenceWrapperPass &mdwp = getAnalysis<MemoryDependenceWrapperPass>();
		LoopAccessLegacyAnalysis &lala = getAnalysis<LoopAccessLegacyAnalysis>();
		DependenceInfo &depInfo = getAnalysis<DependenceAnalysisWrapperPass>().getDI();
		auto &mdr = mdwp.getMemDep();

		// get branch weights
		BlockFrequencyInfo &BFI = getAnalysis<BlockFrequencyInfoWrapperPass>().getBFI();
		//BFI.print(outs());
		const BranchProbabilityInfo *BPI = BFI.getBPI();
		//BPI->print(outs());
		/*	
		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; I++)
			for (inst_iterator J = I; J != E; J++){
				auto infoPtr = depInfo.depends(&*I, &*J, true);
				if (infoPtr != NULL)
				infoPtr->dump(errs());
			}	
		*/
		// output file name
		string baseOutName;	

	
		outs()<<"Function: " << F.getName() << "\n";
		string funcName = F.getName().str();
		// go through loops	
		for (LoopInfo::iterator lp = li.begin(); lp != li.end(); lp++)
		{

			Loop *loop = *lp;
			// find outermost loop
			if (loop -> getParentLoop() != NULL) continue;
			if (baseOutName.empty())
			{
				DISubprogram* dis = F.getSubprogram();
				if (dis){
					string temp = dis->getFilename().str();
					baseOutName = temp.substr(temp.find_last_of("/")+1);
				} 

			}				

			// output name for each loop
			string outFileName = baseOutName;

			DebugLoc sline = loop -> getStartLoc();
			string ll;
			if (sline)
			{
			    ll = to_string(sline.getLine());
				outs() << "Loop starts at line: " << ll <<"\n";
				outFileName.append("."+ll);
				outFileName.append(".dot");
			}	

			// outstream
			error_code RC;	
			raw_fd_ostream out(outFileName, RC, sys::fs::OpenFlags::F_None);
			// write dot file head
			out << "digraph \"CFG for loop in function \'" << funcName << "\' at line " << ll << "\" {\n";
			out << "\tlabel = " << "\"CFG for loop in function \'" << funcName << "\' at line " << ll << "\";\n\n";

			// label each block within the loop
			int label = 0;
			for (Loop::block_iterator bi = loop -> block_begin(); bi != loop -> block_end(); bi++)
			{
				//(*bi)->printAsOperand(outs(),false);
				(*bi) -> setName(Twine(label++));
			}
	
			/*
			float* matrix = new float[label*label];
			for (int i = 0; i < label; i++)
				for (int j = 0; j < label; j++)
				{	
					matrix[i*label + j] = 0.0f;
				}
			*/

			// need to modify the number of children for loop header
			// as 1 since the false branch of loop header is not included
			// in the basic blocks, so the loop header branch is removed

			//matrix[0 * label + 1] = 1.0f;

			for (Loop::block_iterator bi = (loop -> block_begin()); bi != loop -> block_end(); bi++)
			{
				BasicBlock* bb = *bi;				
				int mix[4] = {0};
				getInstrMix(bb, mix); 
				//outs() << "test 1: " << mix[0] << " test2  " << mix[1] << "\n"; 
				// write node info
				out << "\t" << (bb->getName()).str() <<" [shape=record,label=\"{";
				out << (bb->getName()).str() << ":\\l FLOP: " << mix[0] << "\\l IntOp: " << mix[3] <<"\\l MemOp: " << mix[1] << "\\l CtrlOp: " << mix[2] << "\\l";
				const TerminatorInst *tInst = bb -> getTerminator();
				unsigned numSucc = tInst -> getNumSuccessors();
				if (numSucc == 0) 
				{
					// no child, close node info here
					out << "}\"];\n";	
				}

				if (numSucc > 0)
				{
					int pid;
					// parent bb id
					(bb -> getName()).getAsInteger(10, pid);

					if (numSucc == 1)
					{
						// has only one child
						int cid;
						// child bb id
						(tInst -> getSuccessor(0) -> getName()).getAsInteger(10, cid);
						// close node info, add edge info
						out << "}\"];\n";
						out << "\t" << (bb->getName()).str() << " -> " << (tInst->getSuccessor(0)->getName()).str();
						// edge weight is 1.00
						out <<" [label = \"1.00\"];\n";
						//matrix[pid * label + cid] = 1.0f;
					} else {
						// process branch	
						// assume two branch at most for now
							int cid;
						// close node info
						out <<"|{<s0>T|<s1>F}}\"];\n";
		
						for (unsigned i = 0; i < numSucc; i++) {
							BasicBlock *succBB = tInst->getSuccessor(i);
							BranchProbability bp = BPI->getEdgeProbability(bb, succBB);	
							//succBB->printAsOperand(outs(),false);
							float value = rint((float)bp.getNumerator()/bp.getDenominator()*100)/100;
							(succBB -> getName()).getAsInteger(10, cid);
							//matrix[pid * label + cid] = value;
							// write edge
							// for the loop head we need to create a fake node 
							// to represent the node outside loop to complete branch
							string succName = (succBB->getName()).str();
							if (succName.empty())
							{
								out << "\t" << (bb->getName()).str() << ":s" << i << " -> outside";
								out << " [label =\"" << value << "\"];\n";
								// create an outside node
								out << "\toutside [shape=record,label=\"outside loop\"];\n";
								
							} else {
								out << "\t" << (bb->getName()).str() << ":s" << i << " -> " << (succBB->getName()).str();
								out << " [label =\"" << value << "\"];\n";
							}	
						}
					
					}	
				
				}	
			}
			
			/*
			// outstream
			error_code RC;	
			raw_fd_ostream out(outFileName, RC, sys::fs::OpenFlags::F_None);
			// first line of the output file is the matrix size
			out << label <<"\n";
			for (int i = 0; i < label; i++)
			{
				for (int j = 0; j < label; j++)
				{	
					out << matrix[i*label + j] << "\t";
				}
				out <<"\n";
			}
			delete[] matrix;
			*/
			// close graph
			out<<"}";
			out.close();

		}// end for 

		/*
		outs() << "SCCs for " << F.getName() << " in post-order:\n";
		for (scc_iterator<Function *> I = scc_begin(&F),
				IE = scc_end(&F);
				I != IE; ++I) {
			// Obtain the vector of BBs in this SCC and print it out.
			const std::vector<BasicBlock *> &SCCBBs = *I;
			outs() << "  SCC: ";
			for (std::vector<BasicBlock *>::const_iterator BBI = SCCBBs.begin(),
					BBIE = SCCBBs.end();
					BBI != BBIE; ++BBI) {
				outs() << (*BBI)->getName() << "  ";

			}
			outs() << "\n";

		}	
		*/	
	
		/*	
		// instruction mix
		for (Function::iterator fi = F.begin(); fi != F.end(); fi++)
		{
			BasicBlock &bb = *fi;
			bb.setName(Twine(1));
			outs()<<"bb id: ";
			bb.printAsOperand(outs(),false);
			outs()<<"\n";
			// define instruction counters
			int cntMemOp = 0;
			int cntVec = 0;
			// iterate instructions
			for (BasicBlock::iterator bi = bb.begin(); bi != bb.end(); bi++) {
				Instruction &ins = *bi;
				//outs()<<ins.getOpcodeName()<<"\n";					
				if (ins.mayReadOrWriteMemory()) 
				{
					cntMemOp++;
				}
				
				if (ins.getNumOperands() > 0)
				{
					if (ins.getOperand(0)->getType()->isVectorTy())
					{
						outs()<<ins.getOpcodeName()<<"\n";
						outs()<<"type ";
						ins.getType()->print(outs());
						cntVec++;
					}
				}
			}

			outs() << "============== stats ============"<<"\n";
			outs() << "Memory instruction: " << cntMemOp << "\n";
			outs() << "Vector instruction: " << cntVec << "\n";
			outs() << "================================="<<"\n\n";
			
		}
		*/
		

		return false;
	}

  };
}

char CfgPass::ID = 0;

/*
// In this way we can directly use clang to load the dynamic library
// e.g clang++-3.9 -Xclang -load -Xclang pass/libCfgPass.so ../test/HelloWorld.C
 
static void registerCfgPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
	PM.add(new MemoryDependenceWrapperPass());
	PM.add(new LoopAccessLegacyAnalysis());
	PM.add(new CfgPass());
}
static RegisterStandardPasses
  RegisterCfgPass(PassManagerBuilder::EP_EarlyAsPossible,
                 registerCfgPass);
*/

// In this way we should use opt to load the dynamic library and apply to the llvm bitcode
// e.g clang-3.9 -c -emit-llvm ../test/HelloWorld.C -o HelloWorld.bc
// opt-3.9 -mem2reg -loop-simplify -loop-rotate -load pass/libSkeletonPass.so -test -disable-output HelloWorld.bc
static RegisterPass<CfgPass> X("test", "test for using the existing pass",
									"false",
									"true");
