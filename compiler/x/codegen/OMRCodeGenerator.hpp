/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef OMR_X86_CODEGENERATOR_INCL
#define OMR_X86_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGENERATOR_CONNECTOR
#define OMR_CODEGENERATOR_CONNECTOR
namespace OMR { namespace X86 { class CodeGenerator; } }
namespace OMR { typedef OMR::X86::CodeGenerator CodeGeneratorConnector; }
#endif

#include "compiler/codegen/OMRCodeGenerator.hpp"

#include "codegen/Machine.hpp"                 // for Machine, etc
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"                // for Register
#include "codegen/RegisterIterator.hpp"        // for RegisterIterator
#include "codegen/ScratchRegisterManager.hpp"
#include "compile/Compilation.hpp"             // for Compilation
#include "env/jittypes.h"                      // for intptrj_t
#include "il/SymbolReference.hpp"              // for SymbolReference
#include "il/symbol/AutomaticSymbol.hpp"       // for AutomaticSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "infra/BitVector.hpp"                 // for TR_BitVector
#include "infra/TRlist.hpp"
#include "x/codegen/X86Ops.hpp"                // for TR_X86OpCodes
#include "x/codegen/X86Register.hpp"           // for TR_X86FPStackRegister, etc
#include "env/CompilerEnv.hpp"

#ifdef LINUX
#include <sys/time.h>                          // for timeval
#endif

#include "codegen/Instruction.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/GCStackAtlas.hpp"

class TR_GCStackMap;
class TR_IA32ConstantDataSnippet;
class TR_IA32DataSnippet;
class TR_OutlinedInstructions;
namespace OMR { namespace X86 { class CodeGenerator; } }
namespace TR { class CodeGenerator; }
namespace TR { class MemoryReference; }
namespace TR { class X86ImmInstruction;         }
namespace TR { class X86LabelInstruction;       }
namespace TR { class X86MemTableInstruction;    }
namespace TR { class X86ScratchRegisterManager; }
namespace TR { class X86VFPSaveInstruction;     }
struct TR_X86LinkageProperties;

class TR_ClobberingInstruction
   {
   TR::Instruction *_instruction;
   TR::list<TR::Register*> _clobberedRegisters;

public:
   TR_ALLOC(TR_Memory::ClobberingInstruction)

   TR_ClobberingInstruction(TR_Memory * m) : _instruction(0), _clobberedRegisters(getTypedAllocator<TR::Register*>(TR::comp()->allocator())) {};
   TR_ClobberingInstruction(TR::Instruction *instr, TR_Memory * m) : _instruction(instr), _clobberedRegisters(getTypedAllocator<TR::Register*>(TR::comp()->allocator())) {}

   TR::Instruction *getInstruction() {return _instruction;}
   TR::Instruction *setInstruction(TR::Instruction *i) {return (_instruction = i);}
   TR::list<TR::Register*> &getClobberedRegisters() {return _clobberedRegisters;}
   void addClobberedRegister(TR::Register *reg) {_clobberedRegisters.push_front(reg);}
   };

struct TR_BetterSpillPlacement
   {
   TR_ALLOC(TR_Memory::BetterSpillPlacement)

   TR_BetterSpillPlacement *_next;
   TR_BetterSpillPlacement *_prev;
   TR::Register *_virtReg;
   TR::Instruction *_branchInstruction;
   uint32_t _freeRealRegs;
   };

#define CPUID_SIGNATURE_STEPPING_MASK       0x0000000f
#define CPUID_SIGNATURE_MODEL_MASK          0x000000f0
#define CPUID_SIGNATURE_FAMILY_MASK         0x00000f00
#define CPUID_SIGNATURE_PROCESSOR_MASK      0x00003000
#define CPUID_SIGNATURE_EXTENDEDMODEL_MASK  0x000f0000
#define CPUID_SIGNATURE_EXTENDEDFAMILY_MASK 0x0ff00000

struct TR_X86ProcessorInfo
   {
   public:

   TR_ALLOC(TR_Memory::IA32ProcessorInfo)

   enum TR_X86ProcessorVendors
      {
      TR_AuthenticAMD                  = 0x01,
      TR_GenuineIntel                  = 0x02,
      TR_UnknownVendor                 = 0x04
      };

   enum TR_X86ProcessorFeatures
      {
      TR_BuiltInFPU                    = 0x00000001,
      TR_VirtualModeExtension          = 0x00000002,
      TR_DebuggingExtension            = 0x00000004,
      TR_PageSizeExtension             = 0x00000008,
      TR_RDTSCInstruction              = 0x00000010,
      TR_ModelSpecificRegisters        = 0x00000020,
      TR_PhysicalAddressExtension      = 0x00000040,
      TR_MachineCheckException         = 0x00000080,
      TR_CMPXCHG8BInstruction          = 0x00000100,
      TR_APICHardware                  = 0x00000200,
      TR_FastSystemCalls               = 0x00000800,
      TR_MemoryTypeRangeRegisters      = 0x00001000,
      TR_PageGlobalFlag                = 0x00002000,
      TR_MachineCheckArchitecture      = 0x00004000,
      TR_CMOVInstructions              = 0x00008000,
      TR_PageAttributeTable            = 0x00010000,
      TR_36BitPageSizeExtension        = 0x00020000,
      TR_ProcessorSerialNumber         = 0x00040000, // GenuineIntel only
      TR_CLFLUSHInstruction            = 0x00080000, // GenuineIntel only
      // Reserved by Intel             = 0x00100000,
      TR_DebugTraceStore               = 0x00200000, // GenuineIntel only
      TR_ACPIRegisters                 = 0x00400000, // GenuineIntel only
      TR_MMXInstructions               = 0x00800000,
      TR_FastFPSavesRestores           = 0x01000000,
      TR_SSE                           = 0x02000000,
      TR_SSE2                          = 0x04000000, // GenuineIntel Only
      TR_SelfSnoop                     = 0x08000000, // GenuineIntel Only
      TR_HyperThreading                = 0x10000000, // GenuineIntel Only
      TR_ThermalMonitor                = 0x20000000, // GenuineIntel Only
      // Reserved by Intel             = 0x40000000, // IA64-specific
      // Reserved by Intel             = 0x80000000
      TR_X86ProcessorInfoInitialized   = 0x80000000  // FIXME: Using a reserved bit for our purposes.
      };

   enum TR_X86ProcessorFeatures2
      {
      TR_SSE3                          = 0x00000001,
      TR_Monitor                       = 0x00000008,
      TR_SpeedStep                     = 0x00000080,
      TR_CMPXCHG16BInstruction         = 0x00002000,
      TR_SSE4_1                        = 0x00080000,
      TR_SSE4_2                        = 0x00100000,
      TR_POPCNT                        = 0x00800000,
      TR_AESNI                         = 0x02000000
      };


   enum TR_ProcessorDescription
      {
      TR_ProcessorUnknown          = 0x00000000,
      TR_ProcessorIntelPentium     = 0x00000001,
      TR_ProcessorIntelP6          = 0x00000002,
      TR_ProcessorIntelPentium4    = 0x00000003,

      TR_ProcessorAMDK5            = 0x00000004,
      TR_ProcessorAMDK6            = 0x00000005,
      TR_ProcessorAMDAthlonDuron   = 0x00000006,
      TR_ProcessorAMDOpteron       = 0x00000007,

      TR_ProcessorIntelCore2       = 0x00000008,
      TR_ProcessorIntelTulsa       = 0x00000009,
      TR_ProcessorIntelNehalem     = 0x0000000a,

      TR_ProcessorAMDFamily15h     = 0x0000000b,
      TR_ProcessorIntelWestmere    = 0x0000000c,
      TR_ProcessorIntelSandyBridge = 0x0000000d,
      TR_ProcessorIntelIvyBridge   = 0x0000000e,
      TR_ProcessorIntelHaswell     = 0x0000000f
      };


   bool hasBuiltInFPU()                    {return _featureFlags.testAny(TR_BuiltInFPU);}
   bool supportsVirtualModeExtension()     {return _featureFlags.testAny(TR_VirtualModeExtension);}
   bool supportsDebuggingExtension()       {return _featureFlags.testAny(TR_DebuggingExtension);}
   bool supportsPageSizeExtension()        {return _featureFlags.testAny(TR_PageSizeExtension);}
   bool supportsRDTSCInstruction()         {return _featureFlags.testAny(TR_RDTSCInstruction);}
   bool hasModelSpecificRegisters()        {return _featureFlags.testAny(TR_ModelSpecificRegisters);}
   bool supportsPhysicalAddressExtension() {return _featureFlags.testAny(TR_PhysicalAddressExtension);}
   bool supportsMachineCheckException()    {return _featureFlags.testAny(TR_MachineCheckException);}
   bool supportsCMPXCHG8BInstruction()     {return _featureFlags.testAny(TR_CMPXCHG8BInstruction);}
   bool supportsCMPXCHG16BInstruction()     {return _featureFlags2.testAny(TR_CMPXCHG16BInstruction);}
   bool hasAPICHardware()                  {return _featureFlags.testAny(TR_APICHardware);}
   bool hasMemoryTypeRangeRegisters()      {return _featureFlags.testAny(TR_MemoryTypeRangeRegisters);}
   bool supportsPageGlobalFlag()           {return _featureFlags.testAny(TR_PageGlobalFlag);}
   bool hasMachineCheckArchitecture()      {return _featureFlags.testAny(TR_MachineCheckArchitecture);}
   bool supportsCMOVInstructions()         {return _featureFlags.testAny(TR_CMOVInstructions);}
   bool supportsFCOMIInstructions()        {return _featureFlags.testAll(TR_BuiltInFPU | TR_CMOVInstructions);}
   bool hasPageAttributeTable()            {return _featureFlags.testAny(TR_PageAttributeTable);}
   bool has36BitPageSizeExtension()        {return _featureFlags.testAny(TR_36BitPageSizeExtension);}
   bool hasProcessorSerialNumber()         {return _featureFlags.testAny(TR_ProcessorSerialNumber);}
   bool supportsCLFLUSHInstruction()       {return _featureFlags.testAny(TR_CLFLUSHInstruction);}
   bool supportsDebugTraceStore()          {return _featureFlags.testAny(TR_DebugTraceStore);}
   bool hasACPIRegisters()                 {return _featureFlags.testAny(TR_ACPIRegisters);}
   bool supportsMMXInstructions()          {return _featureFlags.testAny(TR_MMXInstructions);}
   bool supportsFastFPSavesRestores()      {return _featureFlags.testAny(TR_FastFPSavesRestores);}
   bool supportsSSE()                      {return _featureFlags.testAny(TR_SSE);}
   bool supportsSSE2()                     {return _featureFlags.testAny(TR_SSE2);}
   bool supportsSSE4_1()                   {return _featureFlags2.testAny(TR_SSE4_1);}
   bool supportsSSE4_2()                   {return _featureFlags2.testAny(TR_SSE4_2);}
   bool supportsAESNI()                    {return _featureFlags2.testAny(TR_AESNI);}
   bool supportsPOPCNT()                   {return _featureFlags2.testAny(TR_POPCNT);}
   bool supportsSelfSnoop()                {return _featureFlags.testAny(TR_SelfSnoop);}
   bool supportsHyperThreading()           {return _featureFlags.testAny(TR_HyperThreading);}
   bool hasThermalMonitor()                {return _featureFlags.testAny(TR_ThermalMonitor);}

   bool supportsMFence()                   {return _featureFlags.testAny(TR_SSE2);}
   bool supportsLFence()                   {return _featureFlags.testAny(TR_SSE2);}
   bool supportsSFence()                   {return _featureFlags.testAny(TR_SSE | TR_MMXInstructions);}
   bool prefersMultiByteNOP()              {return getX86Architecture() && isGenuineIntel() && !isIntelPentium();}

   uint32_t getCPUStepping(uint32_t signature)       {return (signature & CPUID_SIGNATURE_STEPPING_MASK);}
   uint32_t getCPUModel(uint32_t signature)          {return (signature & CPUID_SIGNATURE_MODEL_MASK) >> 4;}
   uint32_t getCPUFamily(uint32_t signature)         {return (signature & CPUID_SIGNATURE_FAMILY_MASK) >> 8;}
   uint32_t getCPUProcessor(uint32_t signature)      {return (signature & CPUID_SIGNATURE_PROCESSOR_MASK) >> 12;}
   uint32_t getCPUExtendedModel(uint32_t signature)  {return (signature & CPUID_SIGNATURE_EXTENDEDMODEL_MASK) >> 16;}
   uint32_t getCPUExtendedFamily(uint32_t signature) {return (signature & CPUID_SIGNATURE_EXTENDEDFAMILY_MASK) >> 20;}

   bool isIntelPentium()      { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelPentium; }
   bool isIntelP6()           { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelP6; }
   bool isIntelPentium4()     { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelPentium4; }
   bool isIntelCore2()        { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelCore2; }
   bool isIntelTulsa()        { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelTulsa; }
   bool isIntelNehalem()      { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelNehalem; }
   bool isIntelWestmere()     { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelWestmere; }
   bool isIntelSandyBridge()  { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelSandyBridge; }
   bool isIntelIvyBridge()    { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelIvyBridge; }
   bool isIntelHaswell()      { return (_processorDescription & 0x000000ff) == TR_ProcessorIntelHaswell; }
   bool isIntelOldMachine() ;

   bool isAMDK6()             { return (_processorDescription & 0x000000fe) == TR_ProcessorAMDK5; } // accept either K5 or K6
   bool isAMDAthlonDuron()    { return (_processorDescription & 0x000000ff) == TR_ProcessorAMDAthlonDuron; }
   bool isAMDOpteron()        { return (_processorDescription & 0x000000ff) == TR_ProcessorAMDOpteron; }
   bool isAMD15h()            { return (_processorDescription & 0x000000ff) == TR_ProcessorAMDFamily15h; }

   bool isGenuineIntel() {return _vendorFlags.testAny(TR_GenuineIntel);}
   bool isAuthenticAMD() {return _vendorFlags.testAny(TR_AuthenticAMD);}

   bool requiresLFENCE() { return false; /* Dummy for now, we may need LFENCE in future processors*/}

   int32_t getX86Architecture() { return (_processorDescription & 0x000000ff);}

private:

   flags8_t   _vendorFlags;
   flags32_t  _featureFlags;  // cache feature flags for re-use
   flags32_t  _featureFlags2;  // cache feature flags 2 for re-use

   uint32_t _processorDescription;

   friend class OMR::X86::CodeGenerator;

   void initialize(TR::Compilation *);
   };

enum TR_PaddingProperties
   {
   TR_NoOpPadding       = 0,
   TR_AtomicNoOpPadding = 1,
   // Note--other possible property flags:
   // - Don't care about condition codes (eg. add eax,0 would be ok)
   // - Not executed (eg. int3 or eyecatcher would be ok)
   };

#define PADDING_TABLE_MAX_ENCODING_LENGTH 9

class TR_X86PaddingTable
   {
   public:

   TR_X86PaddingTable(uint8_t biggestEncoding, uint8_t*** encodings, flags8_t flags, uint16_t sibMask, uint16_t prefixMask):
      _biggestEncoding(biggestEncoding), _encodings(encodings), _flags(flags), _sibMask(sibMask), _prefixMask(prefixMask) {}

   enum { registerMatters=1 }; // for _flags

   uint8_t     _biggestEncoding;
   flags8_t    _flags;
   uint8_t***  _encodings;
   uint16_t    _sibMask;    // 2^n is set if the NOP with size n uses a SIB byte
   uint16_t    _prefixMask; // 2^n is set if the NOP with size n has a prefix byte
   };

struct TR_VFPState
   {
   TR::RealRegister::RegNum _register;
   int32_t _displacement;

   TR_VFPState():_register(TR::RealRegister::NoReg),_displacement(0){} // Assumes noReg is zero
   void operator =  (const TR_VFPState &other){ _register = other._register; _displacement = other._displacement; }
   bool operator == (const TR_VFPState &other) const { return _register == other._register && _displacement == other._displacement; }
   bool operator != (const TR_VFPState &other) const { return !operator==(other); }
   };

namespace OMR
{

namespace X86
{

class OMR_EXTENSIBLE CodeGenerator : public OMR::CodeGenerator
   {

   public:

   TR::Linkage *createLinkage(TR_LinkageConventions lc);
   void beginInstructionSelection();
   void endInstructionSelection();


   static TR_X86ProcessorInfo &getX86ProcessorInfo() {return _targetProcessorInfo;}

   typedef enum
      {
      Backward = 0,
      Forward  = 1
      } RegisterAssignmentDirection;

   void doRegisterAssignment(TR_RegisterKinds kindsToAssign);
   void doBinaryEncoding();

   void doBackwardsRegisterAssignment(TR_RegisterKinds kindsToAssign, TR::Instruction *startInstruction, TR::Instruction *appendInstruction = NULL);

   bool hasComplexAddressingMode() { return true; }
   bool getSupportsBitOpCodes() { return true; }

   bool getSupportsEncodeUtf16LittleWithSurrogateTest();
   bool getSupportsEncodeUtf16BigWithSurrogateTest();

   bool supportsMergingOfHCRGuards();

   void performNonLinearRegisterAssignmentAtBranch(TR::X86LabelInstruction *branchInstruction, TR_RegisterKinds kindsToBeAssigned);
   void prepareForNonLinearRegisterAssignmentAtMerge(TR::X86LabelInstruction *mergeInstruction);

   void processClobberingInstructions(TR_ClobberingInstruction * clobInstructionCursor, TR::Instruction *instructionCursor);

   // different from evaluateNode in that it returns a clobberable register
   TR::Register *shortClobberEvaluate(TR::Node *node);
   TR::Register *intClobberEvaluate(TR::Node *node);
   virtual TR::Register *longClobberEvaluate(TR::Node *node)=0;

   TR::Register *floatClobberEvaluate(TR::Node *node);
   TR::Register *doubleClobberEvaluate(TR::Node *node);

   const TR_X86LinkageProperties &getProperties() { return *_linkageProperties; }

   RegisterAssignmentDirection getAssignmentDirection() {return _assignmentDirection;}
   RegisterAssignmentDirection setAssignmentDirection(RegisterAssignmentDirection d)
      {
      return (_assignmentDirection = d);
      }

   TR::RegisterIterator *getX87RegisterIterator()                          {return _x87RegisterIterator;}
   TR::RegisterIterator *setX87RegisterIterator(TR::RegisterIterator *iter) {return (_x87RegisterIterator = iter);}

   TR::RealRegister *getFrameRegister()                       {return _frameRegister;}
   TR::RealRegister *getMethodMetaDataRegister();

   bool allowGlobalRegisterAcrossBranch(TR_RegisterCandidate *, TR::Node *);

   void buildRegisterMapForInstruction(TR_GCStackMap *map);

   bool processInstruction(TR::Instruction *instr, TR_BitVector ** registerUsageInfo, int32_t &blockNum, int32_t &isFence, bool traceIt);
   uint32_t isPreservedRegister(int32_t regIndex);
   bool isReturnInstruction(TR::Instruction *instr);
   bool isBranchInstruction(TR::Instruction *instr);
   bool isLabelInstruction(TR::Instruction *instr);
   int32_t isFenceInstruction(TR::Instruction *instr);
   bool isAlignmentInstruction(TR::Instruction *instr);
   TR::Instruction *splitEdge(TR::Instruction *cursor, bool isFallThrough, bool needsJump, TR::Instruction *newSplitLabel, TR::list<TR::Instruction*> *jmpInstrs, bool firstJump = false);
   TR::Instruction *splitBlockEntry(TR::Instruction *instr);
   int32_t computeRegisterSaveDescription(TR_BitVector *regs, bool populateInfo = false);
   void processIncomingParameterUsage(TR_BitVector **registerUsageInfo, int32_t blockNum);
   void updateSnippetMapWithRSD(TR::Instruction *cur, int32_t rsd);
   bool isTargetSnippetOrOutOfLine(TR::Instruction *instr, TR::Instruction **start, TR::Instruction **end);
   bool enableAESInHardwareTransformations();

   TR::SymbolReference *getWordConversionTemp();
   TR::SymbolReference *getDoubleWordConversionTemp();

   TR::SymbolReference *findOrCreateCurrentTimeMillisTempSymRef();
   TR::SymbolReference *getNanoTimeTemp();

   int32_t branchDisplacementToHelperOrTrampoline(uint8_t *nextInstructionAddress, TR::SymbolReference *helper);

   // Note: This leaves the code aligned in the specified manner.
   TR::Instruction *generateSwitchToInterpreterPrePrologue(TR::Instruction *prev, uint8_t alignment, uint8_t alignmentMargin);

   int32_t setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart, bool isWarm = 0);
   void emitDataSnippets(bool isWarm = 0);
   bool hasDataSnippets() { return _dataSnippetList.empty() ? false : true; }

   TR::list<TR::Register*> &getSpilledIntRegisters() {return _spilledIntRegisters;}

   TR::list<TR::Register*> &getLiveDiscardableRegisters() {return _liveDiscardableRegisters;}
   void addLiveDiscardableRegister(TR::Register *reg);
   void removeLiveDiscardableRegister(TR::Register *reg);
   void clobberLiveDiscardableRegisters(TR::Instruction  *instr, TR::MemoryReference  *mr);

   bool canTransformUnsafeCopyToArrayCopy();

   using OMR::CodeGenerator::canNullChkBeImplicit;
   bool canNullChkBeImplicit(TR::Node *node);

   TR::list<TR::Register*> &getDependentDiscardableRegisters() {return _dependentDiscardableRegisters;}
   void addDependentDiscardableRegister(TR::Register *reg) {_dependentDiscardableRegisters.push_front(reg);}
   void clobberLiveDependentDiscardableRegisters(TR_ClobberingInstruction *instr, TR::Register *baseReg);
   void reactivateDependentDiscardableRegisters(TR::Register *baseReg);
   void deactivateDependentDiscardableRegisters(TR::Register *baseReg);

   TR::list<TR_ClobberingInstruction*> &getClobberingInstructions() {return _clobberingInstructions;}
   void addClobberingInstruction(TR_ClobberingInstruction *i)  {_clobberingInstructions.push_front(i);}

   TR::list<TR_OutlinedInstructions*> &getOutlinedInstructionsList() {return _outlinedInstructionsList;}

   TR_X86ScratchRegisterManager *generateScratchRegisterManager(int32_t capacity=7);

   // Late edge splitting
   //
   void addDeferredSplit(TR::X86LabelInstruction *branch){ _deferredSplits.push_front(branch); }
   int32_t hasDeferredSplits(){ return !_deferredSplits.empty(); }
   void clearDeferredSplits();
   void performDeferredSplits();
   void processDeferredSplits(bool clear);
   TR::LabelSymbol *splitLabel(TR::LabelSymbol *targetLabel, TR::X86LabelInstruction *instructionToDefer=NULL);

   bool supportsConstantRematerialization();
   bool supportsLocalMemoryRematerialization();
   bool supportsStaticMemoryRematerialization();
   bool supportsIndirectMemoryRematerialization();
   bool supportsAddressRematerialization();
   bool supportsXMMRRematerialization();
   bool supportsFS0VMThreadRematerialization();
   virtual bool allowVMThreadRematerialization();

   TR::Instruction *setLastCatchAppendInstruction(TR::Instruction *i) {return (_lastCatchAppendInstruction=i);}
   TR::Instruction *getLastCatchAppendInstruction()                  {return _lastCatchAppendInstruction;}

   void saveBetterSpillPlacements(TR::Instruction *branchInstruction);
   void removeBetterSpillPlacementCandidate(TR::RealRegister *realReg);
   TR::Instruction *findBetterSpillPlacement(TR::Register *virtReg, int32_t realRegNum);

   int32_t getInstructionPatchAlignmentBoundary()       {return _instructionPatchAlignmentBoundary;}
   void setInstructionPatchAlignmentBoundary(int32_t p) {_instructionPatchAlignmentBoundary = p;}

   int32_t getPicSlotCount()       {return _PicSlotCount;}
   void setPicSlotCount(int32_t c) {_PicSlotCount = c;}
   int32_t incPicSlotCountBy(int32_t i) {return (_PicSlotCount += i);}

   int32_t getLowestCommonCodePatchingAlignmentBoundary() {return _lowestCommonCodePatchingAlignmentBoundary;}
   void setLowestCommonCodePatchingAlignmentBoundary(int32_t b) {_lowestCommonCodePatchingAlignmentBoundary = b;}

   // NOT NEEDED, overridden in amd64/i386
   bool internalPointerSupportImplemented() {return false;} // no virt

   bool supportsSinglePrecisionSQRT() {return true;}

   bool supportsComplexAddressing();

   bool getSupportsNewObjectAlignment() { return true; }
   bool getSupportsTenuredObjectAlignment() { return true; }
   bool isObjectOfSizeWorthAligning(uint32_t size)
      {
      uint32_t lineSize = 64;//getX86ProcessorInfo().getL1DataCacheLineSize();
                             //the query doesn't seem to work on AMD processors;
      return  ((size < (lineSize<<1)) && (size > (lineSize >> 2)));

      }

   int32_t getMaximumNumbersOfAssignableGPRs();
   int32_t getMaximumNumbersOfAssignableFPRs();
   int32_t getMaximumNumbersOfAssignableVRs();
   bool willBeEvaluatedAsCallByCodeGen(TR::Node *node, TR::Compilation *comp);

   uint8_t getSizeOfCombinedBuffer();

   bool suppressInliningOfRecognizedMethod(TR::RecognizedMethod);
   bool supportsInliningOfIsInstance();

   bool supportsPassThroughCopyToNewVirtualRegister() { return true; }

   bool doRematerialization() {return true;}

   bool materializesHeapBase() { return false; }
   bool canFoldLargeOffsetInAddressing() { return true; }

   int32_t arrayInitMinimumNumberOfBytes()
      {
      if (TR::Compiler->target.is64Bit()) return 12;
      return 8;
      }

   int32_t getMaxPatchableInstructionLength() { return 10; }

   // codegen methods referenced from ras/
   //
   uint32_t estimateBinaryLength(TR::MemoryReference *);

   void apply32BitLoadLabelRelativeRelocation(TR::Instruction *movInstruction, TR::LabelSymbol *startLabel, TR::LabelSymbol *endLabel, int32_t deltaToStartLabel);

   void apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);

   bool isAddressScaleIndexSupported(int32_t scale) { if (scale <= 8) return true; return false; }

   void addMetaDataForBranchTableAddress(uint8_t *target, TR::Node *caseNode, TR::X86MemTableInstruction *jmpTableInstruction);


   ///////////////////////////////////
   //
   // No-op generation
   //

   // Emits length bytes of padding starting at cursor with the given properties.
   // Returns the first byte beyond the padding.
   // neighborhood, if provided, provides a context that allows for a good
   // no-op instruction to be chosen based on the surrounding instructions.
   //
   virtual uint8_t *generatePadding(uint8_t              *cursor,
                                    intptrj_t             length,
                                    TR::Instruction    *neighborhood=NULL,
                                    TR_PaddingProperties  properties=TR_NoOpPadding,
                                    bool                  recursive=false);

   // Returns the number of bytes of padding required to cause cursor to be
   // aligned to the given boundary, minus the margin.
   // Specifically, cursor + margin + alignment(cursor, boundary, margin) will
   // be a multiple of boundary.
   //
   intptrj_t alignment(intptrj_t cursor, intptrj_t boundary, intptrj_t margin=0)
      {
      TR_ASSERT((boundary & (boundary-1)) == 0, "Can only align to powers of 2");
      return (-cursor - margin) & (boundary-1);
      }
   intptrj_t alignment(void *cursor, intptrj_t boundary, intptrj_t margin=0);

   bool patchableRangeNeedsAlignment(void *cursor, intptrj_t length, intptrj_t boundary, intptrj_t margin=0);

   bool nopsAlsoProcessedByRelocations() { return false; }

#if defined(DEBUG)
   void dumpPreFPRegisterAssignment(TR::Instruction *);
   void dumpPostFPRegisterAssignment(TR::Instruction *, TR::Instruction *);
   void dumpPreGPRegisterAssignment(TR::Instruction *);
   void dumpPostGPRegisterAssignment(TR::Instruction *, TR::Instruction *);
#endif
#ifdef DEBUG
   void dumpDataSnippets(TR::FILE *pOutFile, bool isWarm = 0);
#endif

   TR_IA32ConstantDataSnippet *findOrCreate2ByteConstant(TR::Node *, int16_t c, bool isWarm = 0);
   TR_IA32ConstantDataSnippet *findOrCreate4ByteConstant(TR::Node *, int32_t c, bool isWarm = 0);
   TR_IA32ConstantDataSnippet *findOrCreate8ByteConstant(TR::Node *, int64_t c, bool isWarm = 0);
   TR_IA32ConstantDataSnippet *findOrCreate16ByteConstant(TR::Node *, void *c, bool isWarm = 0);
   TR_IA32DataSnippet *create2ByteData(TR::Node *, int16_t c, bool isWarm = 0);
   TR_IA32DataSnippet *create4ByteData(TR::Node *, int32_t c, bool isWarm = 0);
   TR_IA32DataSnippet *create8ByteData(TR::Node *, int64_t c, bool isWarm = 0);
   TR_IA32DataSnippet *create16ByteData(TR::Node *, void *c, bool isWarm = 0);

   bool supportsCMOV() {return (_targetProcessorInfo.supportsCMOVInstructions());}

   static TR_X86ProcessorInfo _targetProcessorInfo;

   // The core "clobberEvaluate" logic for single registers (not register
   // pairs), parameterized by the opcode used to move the desired value into a
   // clobberable register if necessary
   TR::Register *gprClobberEvaluate(TR::Node *node, TR_X86OpCodes movRegRegOpCode);

   TR_OutlinedInstructions *findOutlinedInstructionsFromLabel(TR::LabelSymbol *label);
   TR_OutlinedInstructions *findOutlinedInstructionsFromMergeLabel(TR::LabelSymbol *label);
   TR_OutlinedInstructions *findOutlinedInstructionsFromLabelForShrinkWrapping(TR::LabelSymbol *label);

   const TR_VFPState         &vfpState()           { return _vfpState; }
   TR::X86VFPSaveInstruction  *vfpResetInstruction(){ return _vfpResetInstruction; }
   void setVFPState(const TR_VFPState &newState){ _vfpState = newState; }
   void initializeVFPState(TR::RealRegister::RegNum reg, int32_t displacement){ _vfpState._register = reg; _vfpState._displacement = displacement; }


   void removeUnavailableRegisters(TR_RegisterCandidate * rc, TR::Block * * blocks, TR_BitVector & availableRegisters);

   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm);

   bool supportsDebugCounters(TR::DebugCounterInjectionPoint injectionPoint){ return true; }

   virtual uint8_t nodeResultGPRCount  (TR::Node *node, TR_RegisterPressureState *state);

   virtual TR_BitVector *getGlobalRegisters(TR_SpillKinds kind, TR_LinkageConventions lc){ return &_globalRegisterBitVectors[kind]; }

   virtual void simulateNodeEvaluation (TR::Node *node, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary);

   protected:

   CodeGenerator();

   // Note: the following should be called by subclasses near the end of their
   // constructors.  This breaks a rather nasty cyclic initialization dependency,
   // but it is pretty ugly.
   // TODO: Figure out a cleaner solution for this.
   void initialize( TR::Compilation *comp);

   TR::RealRegister::RegNum pickNOPRegister(TR::Instruction  *successor);

   TR_BitVector _globalRegisterBitVectors[TR_numSpillKinds];

   static uint8_t k8PaddingEncoding[PADDING_TABLE_MAX_ENCODING_LENGTH][PADDING_TABLE_MAX_ENCODING_LENGTH];
   static uint8_t intelMultiBytePaddingEncoding[PADDING_TABLE_MAX_ENCODING_LENGTH][PADDING_TABLE_MAX_ENCODING_LENGTH];
   static uint8_t old32BitPaddingEncoding[PADDING_TABLE_MAX_ENCODING_LENGTH][PADDING_TABLE_MAX_ENCODING_LENGTH];

   static TR_X86PaddingTable _intelMultiBytePaddingTable;
   static TR_X86PaddingTable _K8PaddingTable;
   static TR_X86PaddingTable _old32BitPaddingTable;

   TR::X86ImmInstruction *_returnTypeInfoInstruction;
   const TR_X86LinkageProperties  *_linkageProperties;

   private:

   bool nodeIsFoldableMemOperand(TR::Node *node, TR::Node *parent, TR_RegisterPressureState *state);

   TR_IA32ConstantDataSnippet     *findOrCreateConstant(TR::Node *, void *c, uint8_t size, bool isWarm=0);

   TR::RealRegister             *_frameRegister;

   TR::SymbolReference             *_wordConversionTemp;
   TR::SymbolReference             *_doubleWordConversionTemp;
   TR::SymbolReference             *_currentTimeMillisTemp;
   TR::SymbolReference             *_nanoTimeTemp;

   TR::Instruction                 *_lastCatchAppendInstruction;
   TR_BetterSpillPlacement        *_betterSpillPlacements;

   TR::list<TR_IA32DataSnippet*>          _dataSnippetList;
   TR::list<TR::Register*>               _spilledIntRegisters;
   TR::list<TR::Register*>               _liveDiscardableRegisters;
   TR::list<TR::Register*>               _dependentDiscardableRegisters;
   TR::list<TR_ClobberingInstruction*>  _clobberingInstructions;
   std::list<TR_ClobberingInstruction*, TR::typed_allocator<TR_ClobberingInstruction*, TR::Allocator> >::iterator _clobIterator;
   TR::list<TR::X86LabelInstruction*>    _deferredSplits;
   TR::list<TR_OutlinedInstructions*>   _outlinedInstructionsList;

   RegisterAssignmentDirection     _assignmentDirection;
   TR::RegisterIterator            *_x87RegisterIterator;

   int32_t                         _instructionPatchAlignmentBoundary;
   int32_t                         _PicSlotCount;
   int32_t                         _lowestCommonCodePatchingAlignmentBoundary;

   TR_VFPState                     _vfpState;
   TR::X86VFPSaveInstruction       *_vfpResetInstruction;

   TR_X86PaddingTable             *_paddingTable;

   TR::LabelSymbol                  *_switchToInterpreterLabel;

   TR_X86OpCodes                   _xmmDoubleLoadOpCode;

   enum TR_X86CodeGeneratorFlags
      {
      EnableBetterSpillPlacements              = 0x00000001, ///< use better spill placements
      EnableRematerialisation                  = 0x00000002, ///< use register rematerialisation
      EnableRegisterAssociations               = 0x00000004, ///< use register associations for register assignment
      EnableSinglePrecisionMethods             = 0x00000008, ///< support changing FPCW to single precision for individual methods
      EnableRegisterInterferences              = 0x00000010, ///< consider register interferences during register assignment
      EnableRegisterWeights                    = 0x00000020, ///< use register weights in choosing a best register candidate
      UseSSEForSinglePrecision                 = 0x00000040, ///< use SSE extensions for single precision
      UseSSEForDoublePrecision                 = 0x00000080, ///< use SSE extensions for double precision
      EnableImplicitDivideCheck                = 0x00000100, ///< platform can catch hardware exceptions for divide overflow and divide by zero
      GenerateMasmListingSyntax                = 0x00000200, ///< generate Masm-style syntax in the debug listings
      MapAutosTo8ByteSlots                     = 0x00000400, ///< don't round up sizes of autos to an 8-byte slot size when the stack is mapped
      EnableTLHPrefetching                     = 0x00000800, ///< enable software prefetches on TLH allocates
      UseGPRsForWin32CTMConversion             = 0x00001000, ///< use magic number approach for long division for CTM conversion to milliseconds
      UseLongDivideHelperForWin32CTMConversion = 0x00002000, ///< use long division helper for CTM conversion to milliseconds
      TargetSupportsSoftwarePrefetches         = 0x00004000, ///< target processor and OS both support software prefetch instructions
      MethodEnterExitTracingEnabled            = 0x00008000, ///< trace method enter/exits
      // Available                             = 0x00010000,
      PushPreservedRegisters                   = 0x00020000  ///< we've chosen to save/restore preserved regs using push/pop instructions instead of movs
      };

   flags32_t _flags;

   public:

   bool allowHCRGuardMerging() { return false; }

   bool enableBetterSpillPlacements()
      {
      return _flags.testAny(EnableBetterSpillPlacements);
      }
   void setEnableBetterSpillPlacements() {_flags.set(EnableBetterSpillPlacements);}
   void resetEnableBetterSpillPlacements() {_flags.reset(EnableBetterSpillPlacements);}

   bool enableRematerialisation()
      {
      return _flags.testAny(EnableRematerialisation);
      }
   void setEnableRematerialisation() {_flags.set(EnableRematerialisation);}
   void resetEnableRematerialisation() {_flags.reset(EnableRematerialisation);}

   bool enableSinglePrecisionMethods()
      {
      return _flags.testAny(EnableSinglePrecisionMethods);
      }
   void setEnableSinglePrecisionMethods() {_flags.set(EnableSinglePrecisionMethods);}

   bool enableRegisterWeights()
      {
      return _flags.testAny(EnableRegisterWeights);
      }
   void setEnableRegisterWeights() {_flags.set(EnableRegisterWeights);}

   bool enableRegisterInterferences()
      {
      return _flags.testAny(EnableRegisterInterferences);
      }
   void setEnableRegisterInterferences() {_flags.set(EnableRegisterInterferences);}

   bool enableRegisterAssociations()
      {
      return _flags.testAny(EnableRegisterAssociations);
      }
   void setEnableRegisterAssociations() {_flags.set(EnableRegisterAssociations);}

   bool useSSEForSinglePrecision()
      {
      return _flags.testAny(UseSSEForSinglePrecision);
      }
   void setUseSSEForSinglePrecision() {_flags.set(UseSSEForSinglePrecision);}

   bool useSSEForDoublePrecision()
      {
      return _flags.testAny(UseSSEForDoublePrecision);
      }
   void setUseSSEForDoublePrecision() {_flags.set(UseSSEForDoublePrecision);}

   bool useSSEForSingleAndDoublePrecision()
      {
      return _flags.testAll(UseSSEForSinglePrecision | UseSSEForDoublePrecision);
      }

   bool useSSEFor(TR::DataTypes type);

   bool useGPRsForWin32CTMConversion()
      {
      return _flags.testAny(UseGPRsForWin32CTMConversion);
      }
   void setUseGPRsForWin32CTMConversion() {_flags.set(UseGPRsForWin32CTMConversion);}

   bool useLongDivideHelperForWin32CTMConversion()
      {
      return _flags.testAny(UseLongDivideHelperForWin32CTMConversion);
      }
   void setUseLongDivideHelperForWin32CTMConversion() {_flags.set(UseLongDivideHelperForWin32CTMConversion);}

   bool targetSupportsSoftwarePrefetches()
      {
      return _flags.testAny(TargetSupportsSoftwarePrefetches);
      }
   void setTargetSupportsSoftwarePrefetches() {_flags.set(TargetSupportsSoftwarePrefetches);}

   bool enableTLHPrefetching()
      {
      return _flags.testAny(EnableTLHPrefetching);
      }
   void setEnableTLHPrefetching() {_flags.set(EnableTLHPrefetching);}

   bool needToAvoidCommoningInGRA();

   bool generateMasmListingSyntax()    {return _flags.testAny(GenerateMasmListingSyntax);}
   void setGenerateMasmListingSyntax() {_flags.set(GenerateMasmListingSyntax);}

   bool codegenSupportsUnsignedIntegerDivide() {return true;}

   virtual bool supportsDirectJNICallsForAOT() { return true; }

   bool enableImplicitDivideCheck()    { return _flags.testAny(EnableImplicitDivideCheck); }
   void setEnableImplicitDivideCheck() { _flags.set(EnableImplicitDivideCheck); }

   bool mapAutosTo8ByteSlots()     { return _flags.testAny(MapAutosTo8ByteSlots); }
   void setMapAutosTo8ByteSlots()  { _flags.set(MapAutosTo8ByteSlots); }

   TR::X86ImmInstruction* getReturnTypeInfoInstruction() { return _returnTypeInfoInstruction; }

   TR_X86OpCodes getXMMDoubleLoadOpCode() {return _xmmDoubleLoadOpCode;}
   void setXMMDoubleLoadOpCode(TR_X86OpCodes o) {_xmmDoubleLoadOpCode = o;}

   TR::LabelSymbol *getSwitchToInterpreterLabel() {return _switchToInterpreterLabel;}
   void setSwitchToInterpreterLabel(TR::LabelSymbol *s) {_switchToInterpreterLabel = s;}

   bool methodEnterExitTracingEnabled()
      {
      return _flags.testAny(MethodEnterExitTracingEnabled);
      }
   void setMethodEnterExitTracingEnabled() {_flags.set(MethodEnterExitTracingEnabled);}

   bool pushPreservedRegisters()
      { return _flags.testAny(PushPreservedRegisters); }
   void setPushPreservedRegisters() {_flags.set(PushPreservedRegisters);}

   // arrayTranslateTableRequiresAlignment is N/A, since we don't have
   // arraytranslate tables.
   int32_t arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget);
   int32_t arrayTranslateAndTestMinimumNumberOfIterations();
   };

}

}


// Trampolines
//

#if defined(TR_TARGET_64BIT)
#define NEEDS_TRAMPOLINE(target, rip, cg) (cg->alwaysUseTrampolines() || !IS_32BIT_RIP((target), (rip)))
#else
// Give the C++ compiler a hand
#define NEEDS_TRAMPOLINE(target, rip, cg) (0)
#endif

#define IS_32BIT_RIP(x,rip)  ((intptrj_t)(x) == (intptrj_t)(rip) + (int32_t)((intptrj_t)(x) - (intptrj_t)(rip)))


class TR_X86FPStackIterator : public TR::RegisterIterator
   {
   public:

   TR_X86FPStackIterator(TR::Machine *machine, TR_RegisterKinds kind = TR_NoRegister):
      TR::RegisterIterator(machine, kind)
      {
      _machine = machine;
      _cursor = TR_X86FPStackRegister::fpFirstStackReg;
      }

   TR::Register *getFirst() {return _machine->getFPStackLocationPtr(_cursor = TR_X86FPStackRegister::fpFirstStackReg);}
   TR::Register *getCurrent() {return _machine->getFPStackLocationPtr(_cursor);}
   TR::Register *getNext() {return _cursor > TR_X86FPStackRegister::fpLastStackReg ? NULL : _machine->getFPStackLocationPtr(++_cursor);}

   private:

   TR::Machine *_machine;
   int32_t _cursor;
   };

class TR_X86ScratchRegisterManager: public TR_ScratchRegisterManager
   {
   public:

   TR_X86ScratchRegisterManager(int32_t capacity, TR::CodeGenerator *cg) : TR_ScratchRegisterManager(capacity, cg){}
   bool reclaimAddressRegister(TR::MemoryReference *mr);
   };

#endif
