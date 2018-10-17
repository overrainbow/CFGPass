# CFGPass
---------
An LLVM pass for generating control flow graph (CFG) with detail information. 

## Requirements
LLVM (Clang) 3.9 or later
CMake

## Usage

### Prepare target program
1. Instrument source file by Clang-3.9:

```
	clang++-3.9 -O1 -fprofile-instr-generate HelloWorld.C -o HelloWorld_instru
```

2. Run executable to generate profile data:

```
	LLVM_PROFILE_FILE="code-%p.profraw" ./HelloWorld_instru
```

3. Merge profile data from multiple runs:

```
llvm-profdata merge -output=code.profdata code-*.profraw
```

4. Apply profile data to generate bitcode:

```
clang++-3.9 -O1 -c -g -emit-llvm -fprofile-instr-use=code.profdata -o HelloWorld.instru.bc HelloWorld.C
```

5. Generate readably assembly (Optional):

```
clang++-3.9 -S -emit-llvm -fprofile-instr-use=code.profdata -o HelloWorld.instru.ll HelloWorld.c
```

### Generate CFG

```
opt-3.9 -load pass/libCfgPass.so -test -disable-output HelloWorld.instru.bc
```
