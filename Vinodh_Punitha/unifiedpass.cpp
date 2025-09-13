/**
 * unifiedpass.cpp - LCM Implementation with Dataflow Printing (LLVM 17)
 *
 */

// LLVM Headers
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Config/llvm-config.h" // Needed for LLVM_EXTERNAL_VISIBILITY, LLVM_VERSION_STRING
#include "llvm/IR/Argument.h"       // For isa<Argument> in getShortValueName
#include "llvm/IR/CFG.h"            // For predecessor/successor iteration
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"   // Specifically for BinaryOperator etc., PHINode
#include "llvm/IR/IRBuilder.h"      // Needed for inserting instructions
#include "llvm/IR/PassManager.h"    // For new Pass Manager integration
#include "llvm/IR/ValueMap.h"
#include "llvm/Pass.h"              // Includes AnalysisInfoMixin
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"       // For dyn_cast
#include "llvm/Support/raw_ostream.h"   // For printing (outs(), errs())
#include "llvm/ADT/Hashing.h"       // For hash_combine
#include "llvm/Analysis/AliasAnalysis.h" // Included for FunctionAnalysisManager
#include "llvm/Support/Compiler.h" // For LLVM_ATTRIBUTE_UNUSED
#include "llvm/IR/Dominators.h" // *** CORRECTED Include Path for DominatorTree/Analysis ***
#include "llvm/Transforms/Utils/Local.h" // For RecursivelyDeleteTriviallyDeadInstructions

// Standard Library Headers
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <typeinfo> // For demangle
#include <memory>   // For unique_ptr
#include <cassert>  // For assert
#include <algorithm> // For std::find_if

// Demangling for readable pass names (optional but helpful)
#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>
std::string demangle(const char* name) {
    int status = -4; // some arbitrary value to eliminate the compiler warning
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
    };
    return (status==0) ? res.get() : name ;
}
#else
std::string demangle(const char* name) {
    return name;
}
#endif


using namespace llvm;
using namespace std;

//==================== UTILITY CODE ====================//
// ... (getShortValueName, Expression, DenseMapInfo<Expression> ) ...
// Corrected getShortValueName function
std::string getShortValueName(Value *v) {
    if (!v) return "<null>";
    // Special handling for DenseMap empty/tombstone markers if they somehow escape
    if (v == DenseMapInfo<Value *>::getEmptyKey()) {
       return "<empty_marker>";
    }
     if (v == DenseMapInfo<Value *>::getTombstoneKey()) {
       return "<tombstone_marker>";
    }

    if (v->hasName()) { return v->getName().str(); }
    if (isa<ConstantInt>(v) || isa<ConstantFP>(v)) {
        std::string s; raw_string_ostream strm(s); v->printAsOperand(strm, false); return strm.str();
    }
     if (isa<Instruction>(v) || isa<Argument>(v)) {
        std::string s; raw_string_ostream strm(s); v->printAsOperand(strm, false); return strm.str();
    }
    std::string s; raw_string_ostream strm(s); strm << "<" << (void*)v << ">"; return strm.str();
}


// Define Expression class fully BEFORE DenseMapInfo specialization
class Expression {
    // Helper constants for DenseMap markers
    static const Value* EmptyMarker;
    static const Value* TombstoneMarker;

public:
  Value *v1 = nullptr; Value *v2 = nullptr; Instruction::BinaryOps op;
  Instruction* definingInst = nullptr; // Keep track of the original instruction if created from one

  Expression() : v1(nullptr), v2(nullptr), op(Instruction::BinaryOpsEnd), definingInst(nullptr) {}

  Expression(Instruction *I) : v1(nullptr), v2(nullptr), op(Instruction::BinaryOpsEnd), definingInst(nullptr) {
    if (I == reinterpret_cast<Instruction*>(const_cast<Value*>(EmptyMarker)) ||
        I == reinterpret_cast<Instruction*>(const_cast<Value*>(TombstoneMarker)) ||
        !I) {
        return;
    }
    if (auto *BO = dyn_cast<BinaryOperator>(I)) {
        v1 = BO->getOperand(0); v2 = BO->getOperand(1); op = BO->getOpcode(); definingInst = I;
    }
  }

  explicit Expression(int marker_type) : v1(nullptr), v2(nullptr), op(Instruction::BinaryOpsEnd), definingInst(nullptr) {
      if (marker_type == 0) { v1 = const_cast<Value*>(EmptyMarker); }
      else if (marker_type == 1) { v1 = const_cast<Value*>(TombstoneMarker); }
  }

  bool isValid() const {
      return v1 != const_cast<Value*>(EmptyMarker) && v1 != const_cast<Value*>(TombstoneMarker) && v1 != nullptr && v2 != nullptr && op != Instruction::BinaryOps::BinaryOpsEnd;
  }
  bool isEmptyKey() const { return v1 == const_cast<Value*>(EmptyMarker); }
  bool isTombstoneKey() const { return v1 == const_cast<Value*>(TombstoneMarker); }

  struct Hash {
      size_t operator()(const Expression& e) const {
          if (e.isEmptyKey()) return static_cast<size_t>(-1);
          if (e.isTombstoneKey()) return static_cast<size_t>(-2);
          return hash_combine((size_t)e.op, hash_value(e.v1), hash_value(e.v2));
      }
  };

  bool operator==(const Expression &e2) const {
      if (isEmptyKey()) return e2.isEmptyKey();
      if (isTombstoneKey()) return e2.isTombstoneKey();
      if (e2.isEmptyKey() || e2.isTombstoneKey()) return false;
      return (op == e2.op && v1 == e2.v1 && v2 == e2.v2);
  }

  bool operator<(const Expression &e2) const {
    if (isValid() && !e2.isValid()) return true;
    if (!isValid() && e2.isValid()) return false;
    if (!isValid() && !e2.isValid()) {
        if (isEmptyKey() && e2.isTombstoneKey()) return true;
        return false;
    }
    if (op != e2.op) return op < e2.op;
    if (v1 != e2.v1) return v1 < e2.v1;
    return v2 < e2.v2;
  }

  std::string toString() const {
    if (isEmptyKey()) return "<empty_key>";
    if (isTombstoneKey()) return "<tombstone_key>";
    if (!isValid()) return "<invalid expr>";
    std::string opStr;
    switch (op) {
      case Instruction::Add: opStr = "+"; break; case Instruction::Sub: opStr = "-"; break;
      case Instruction::Mul: opStr = "*"; break; case Instruction::SDiv: opStr = "/"; break;
      case Instruction::UDiv: opStr = "/u"; break; case Instruction::SRem: opStr = "%"; break;
      case Instruction::URem: opStr = "%u"; break; case Instruction::Shl: opStr = "<<"; break;
      case Instruction::LShr: opStr = ">>l"; break; case Instruction::AShr: opStr = ">>a"; break;
      case Instruction::And: opStr = "&"; break; case Instruction::Or: opStr = "|"; break;
      case Instruction::Xor: opStr = "^"; break; default: opStr = "<op" + std::to_string(op) + ">"; break;
    }
    return getShortValueName(v1) + " " + opStr + " " + getShortValueName(v2);
  }
};

const Value* Expression::EmptyMarker = DenseMapInfo<Value*>::getEmptyKey();
const Value* Expression::TombstoneMarker = DenseMapInfo<Value*>::getTombstoneKey();

namespace llvm {
template <> struct DenseMapInfo<Expression> {
  static inline Expression getEmptyKey() { return Expression(0); }
  static inline Expression getTombstoneKey() { return Expression(1); }
  static unsigned getHashValue(const Expression &E) { return Expression::Hash()(E); }
  static bool isEqual(const Expression &LHS, const Expression &RHS) { return LHS == RHS; }
};
} // namespace llvm


//==================== DATAFLOW FRAMEWORK CODE ====================//
class Dataflow {
public:
  enum Direction { FORWARD, BACKWARD };
  enum Initial { EMPTY, ALL };

  struct BlockState {
    BasicBlock *bb = nullptr;
    BitVector In;
    BitVector Out;
    unsigned domainSize = 0;

    void initialize(unsigned size, Initial initialVal) {
      domainSize = size;
      if (initialVal == EMPTY) { In = BitVector(size); Out = BitVector(size); }
      else { In = BitVector(size, true); Out = BitVector(size, true); }
    }
  };

  using MeetOpFn = std::function<BitVector(BitVector, BitVector)>;
  using TransferFn = std::function<BitVector(BasicBlock*, const BitVector&)>;

  Dataflow(Direction direction = FORWARD, Initial boundary = EMPTY, Initial initial = EMPTY,
           MeetOpFn meetOp = nullptr, TransferFn transferFn = nullptr)
      : direction(direction), boundary(boundary), initial(initial),
        meetOp(meetOp), transferFn(transferFn), nBlockBits(0) {}

  Dataflow &setDirection(Direction dir) { direction = dir; return *this; }
  Dataflow &setBoundary(Initial b) { boundary = b; return *this; }
  Dataflow &setInitial(Initial i) { initial = i; return *this; }
  Dataflow &setMeetOp(MeetOpFn fn) { meetOp = fn; return *this; }
  Dataflow &setTransferFn(TransferFn fn) { transferFn = fn; return *this; }
  Dataflow &setMeetIdentity(const BitVector& identity) { meetIdentity = identity; return *this; }

  void initializeDomain(unsigned size) {
      nBlockBits = size;
      if (meetIdentity.empty()) { meetIdentity = (initial == ALL) ? BitVector(nBlockBits, true) : BitVector(nBlockBits); }
      else if (meetIdentity.size() != size) {
          meetIdentity.resize(size);
          meetIdentity = (initial == ALL) ? BitVector(nBlockBits, true) : BitVector(nBlockBits);
      }
  }

  void run(Function &F, StringRef debugName = "");

  DenseMap<BasicBlock *, BlockState> &getStates() { return states; }
  const DenseMap<BasicBlock *, BlockState> &getStates() const { return states; }

  const BlockState& getState(BasicBlock* bb) const {
      auto it = states.find(bb);
      if (it != states.end()) { return it->second; }
      else { static BlockState dummy; /* Consider initializing dummy */ return dummy; }
  }

  static std::string bitVectorExprToString( const BitVector &bv, const std::vector<Expression> &exprVec, std::string delimiter = ", ") {
      std::string s = ""; bool first = true;
      for(int i = 0; i < bv.size(); ++i) {
          if (bv[i]) {
              if (!first) { s += delimiter; }
              if (i >= 0 && i < (int)exprVec.size() && exprVec[i].isValid()) {
                 s += exprVec[i].toString();
              } else {
                 s += "<invalid_idx_or_expr_" + std::to_string(i) + ">";
              }
              first = false;
          }
      } return s;
  }
  static std::string bitVectorIndicesToString(const BitVector &bv) {
      std::string s = "{"; bool first = true;
      for(int i = 0; i < bv.size(); ++i) {
          if (bv[i]) { if (!first) s += ", "; s += std::to_string(i); first = false; }
      } s += "}"; return s;
  }

private:
  Direction direction; Initial boundary; Initial initial; unsigned int nBlockBits;
  MeetOpFn meetOp; TransferFn transferFn;
  BitVector meetIdentity;
  DenseMap<BasicBlock *, BlockState> states;
};

void Dataflow::run(Function &F, StringRef debugName) {
  if (nBlockBits == 0) { errs() << "Warning: Dataflow domain size is 0 for " << debugName << ". Analysis not run.\n"; return; }
  if (!meetOp || !transferFn) { errs() << "Error: Meet or Transfer function not set for " << debugName << ".\n"; return; }
  if (meetIdentity.size() != nBlockBits) {
      errs() << "Warning: Meet identity size mismatch for " << debugName << ". Re-initializing based on 'initial'.\n";
      meetIdentity = (initial == ALL) ? BitVector(nBlockBits, true) : BitVector(nBlockBits);
  }

  states.clear(); SmallVector<BasicBlock*, 16> worklist; DenseSet<BasicBlock*> worklistSet;

  for (BasicBlock &block : F) {
    auto [it, inserted] = states.insert({&block, BlockState()});
    BlockState& state = it->second; state.bb = &block;
    if (inserted || state.domainSize != nBlockBits) { state.initialize(nBlockBits, initial); }
    // Apply boundary conditions
    if (direction == FORWARD) { if (pred_begin(&block) == pred_end(&block)) { state.In = (boundary == EMPTY) ? BitVector(nBlockBits) : BitVector(nBlockBits, true); } }
    else { /* BACKWARD */ if (succ_begin(&block) == succ_end(&block)) { state.Out = (boundary == EMPTY) ? BitVector(nBlockBits) : BitVector(nBlockBits, true); } }
    // Add to worklist if not already present
    if (worklistSet.find(&block) == worklistSet.end()) { worklist.push_back(&block); worklistSet.insert(&block); }
  }

  while (!worklist.empty()) {
    BasicBlock *block = worklist.pop_back_val(); worklistSet.erase(block);
    BlockState &st = states[block]; BitVector oldVal; BitVector currentMeetVal;

    if (direction == FORWARD) {
      oldVal = st.Out;
      // Calculate IN = meet(OUT[p]) for all p in predecessors(block)
      if (pred_begin(block) != pred_end(block)) {
          currentMeetVal = meetIdentity; // Initialize with meet identity
          for (BasicBlock *pred : predecessors(block)) {
              auto pred_it = states.find(pred);
              if (pred_it != states.end()) { currentMeetVal = meetOp(currentMeetVal, pred_it->second.Out); }
              else { errs() << "Warning: State not found for predecessor in " << debugName << "\n"; }
          }
          st.In = currentMeetVal; // Update IN set
      } // else: Entry block, IN already set by boundary condition

      // Calculate OUT = transfer(block, IN)
      st.Out = transferFn(block, st.In);

      // If OUT changed, add successors to worklist
      if (st.Out != oldVal) { for (BasicBlock *succ : successors(block)) { if (worklistSet.find(succ) == worklistSet.end()) { worklist.push_back(succ); worklistSet.insert(succ); } } }
    } else { // BACKWARD
      oldVal = st.In;
       // Calculate OUT = meet(IN[s]) for all s in successors(block)
      if (succ_begin(block) != succ_end(block)) {
          currentMeetVal = meetIdentity; // Initialize with meet identity
          for (BasicBlock *succ : successors(block)) {
              auto succ_it = states.find(succ);
              if (succ_it != states.end()) { currentMeetVal = meetOp(currentMeetVal, succ_it->second.In); }
              else { errs() << "Warning: State not found for successor in " << debugName << "\n"; }
          }
           st.Out = currentMeetVal; // Update OUT set
      } // else: Exit block, OUT already set by boundary condition

      // Calculate IN = transfer(block, OUT)
      st.In = transferFn(block, st.Out);

      // If IN changed, add predecessors to worklist
      if (st.In != oldVal) { for (BasicBlock *pred : predecessors(block)) { if (worklistSet.find(pred) == worklistSet.end()) { worklist.push_back(pred); worklistSet.insert(pred); } } }
    }
  }
}


//==================== ANALYSIS PASSES (NEW PM STRUCTURE) ====================//
namespace UnifiedPass {

class AvailableExpressions;
class AnticipatedExpressions;
class UsedExpressions;
class PostponableExpressions; // Forward declaration
class LazyCodeMotion;

// Base class for Analysis Passes
class AnalysisPassBase {
public:
    // Common domain representation
    std::map<Expression, int> exprMap;
    std::vector<Expression> exprVec;
    unsigned numExpr = 0;

    // GEN/KILL sets specific to each analysis
    // Note: For Postponable, KILL depends on UsedExpressions result,
    // so it won't be stored here directly.
    DenseMap<BasicBlock*, BitVector> genSets;
    DenseMap<BasicBlock*, BitVector> killSets; // Used by Avail, Anticip, Used

    // Dataflow framework instance to store results (IN/OUT sets)
    Dataflow df;

    // Default constructor and move semantics
    AnalysisPassBase() = default;
    AnalysisPassBase(const AnalysisPassBase&) = delete; // Prevent accidental copying
    AnalysisPassBase& operator=(const AnalysisPassBase&) = delete;
    AnalysisPassBase(AnalysisPassBase&&) = default; // Allow moving
    AnalysisPassBase& operator=(AnalysisPassBase&&) = default;

    // Build the domain of expressions for the given function
    void buildExpressionDomain(Function &F) {
        exprMap.clear(); exprVec.clear(); numExpr = 0; int idx = 0;
        for (auto &BB : F) { for (auto &I : BB) { if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
            Expression expr(&I); if (!expr.isValid()) continue;
            auto result = exprMap.insert({expr, idx}); // Use insert to check uniqueness
            if (result.second) { // If inserted successfully (was unique)
                exprVec.push_back(expr);
                idx++;
            }
        }}}
        numExpr = exprVec.size();
    }

    // Abstract method to be implemented by derived analysis passes
    // Calculates GEN/KILL where possible (may only calculate GEN if KILL depends on other analyses)
    virtual void calculateGenKillSets(Function &F) = 0;

    // Printing function (shared by all analysis passes)
    void printDataflowResults(Function &F) {
        outs() << "\n=================================================\n";
        // Use RTTI to get the actual analysis pass name
        outs() << "Dataflow Results for: UnifiedPass::" << demangle(typeid(*this).name()) << "\n";
        outs() << "Function: " << F.getName() << "\n";
        outs() << "-------------------------------------------------\n";
        // Print the expression domain mapping
        outs() << "Expression Domain (Index: Expression):\n";
        for(size_t i = 0; i < exprVec.size(); ++i) {
             if (i < exprVec.size() && exprVec[i].isValid()) {
                outs() << "  " << i << ": " << exprVec[i].toString() << "\n";
            } else {
                 outs() << "  " << i << ": <invalid expression in vector index " << i << ">\n";
            }
        }
        outs() << "-------------------------------------------------\n\n";

        for (auto &BB : F) {
            BasicBlock* B = &BB;
            // Get state, gen, and kill for the current block
            auto state_it = df.getStates().find(B);
            auto gen_it = genSets.find(B);
            // KILL set might not be stored for Postponable, handle gracefully
            auto kill_it = killSets.find(B);

            outs() << "Basic Block ";
            if (B->hasName()) { outs() << B->getName(); }
            else { outs() << "<bb " << (void*)B << ">"; }
            outs() << "\n";

            // Print Gen set
            outs() << "  gen\t"
                   << (gen_it != genSets.end() ? Dataflow::bitVectorExprToString(gen_it->second, exprVec) : "<Not Found>")
                   << "\n";

            // Print Kill set (if available in killSets map)
            // For Postponable, KILL = USED_IN, which isn't stored here, so this will show <Not Found> or be empty.
            // Consider adding USED_IN printout within Postponable's run if needed for clarity.
            outs() << "  kill\t"
                   << (kill_it != killSets.end() ? Dataflow::bitVectorExprToString(kill_it->second, exprVec) : "") // Print empty if not found
                   << "\n";

            // Print In/Out sets (if state was computed)
            if (state_it == df.getStates().end()) {
                 outs() << "  State not found!\n";
            } else {
                auto& state = state_it->second; // Use iterator result
                outs() << "  In\t"   << Dataflow::bitVectorExprToString(state.In, exprVec) << "\n";
                outs() << "  Out\t"  << Dataflow::bitVectorExprToString(state.Out, exprVec) << "\n";
            }
            outs() << "---\n"; // Separator
        }
        outs() << "=================================================\n\n";
        outs().flush(); // Ensure output is visible immediately
     }
};

//-----------------------------------------------------------------------------
// 1) Available Expressions Analysis Pass (New PM)
//-----------------------------------------------------------------------------
class AvailableExpressions : public AnalysisPassBase, public AnalysisInfoMixin<AvailableExpressions> {
public:
    friend AnalysisInfoMixin<AvailableExpressions>;
    static AnalysisKey Key;
    using Result = AvailableExpressions; // Result type is the analysis itself

    AvailableExpressions() = default; // Use default constructor

    // Implement GEN/KILL calculation for Available Expressions
    void calculateGenKillSets(Function &F) override {
      genSets.clear(); killSets.clear();
      for (auto &BB : F) {
          BitVector gen_b(numExpr); BitVector kill_b(numExpr);
          for (auto &I : BB) {
              // Check if I kills any expressions by redefining an operand
              Value* definedValue = nullptr;
              // Simple check: if I is an instruction that defines a value (isn't void, store, term, phi, cmp)
              // More complex checks might be needed for memory operations.
              if (!I.getType()->isVoidTy() && !isa<StoreInst>(&I) && !I.isTerminator() && !isa<PHINode>(&I) && !isa<CmpInst>(&I)) {
                   definedValue = &I;
              }
              if (definedValue) {
                  for (size_t i = 0; i < exprVec.size(); ++i) {
                       // Ensure exprVec[i] is valid before accessing members
                       if (i < exprVec.size() && exprVec[i].isValid()) {
                           if (exprVec[i].v1 == definedValue || exprVec[i].v2 == definedValue) {
                               kill_b.set(i); // Definition kills expressions using the defined value
                           }
                       }
                   }
              }

              // Check if I generates an expression
              if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
                  Expression currentExpr(&I); if (!currentExpr.isValid()) continue;
                  auto it = exprMap.find(currentExpr);
                  if (it != exprMap.end()) {
                      // Generate the expression
                      gen_b.set(it->second);
                      // An instruction generating an expression cannot kill it within the same instruction
                      kill_b.reset(it->second);
                  }
              }
          }
          genSets[&BB] = gen_b; killSets[&BB] = kill_b;
      }
     }

    // Meet operator: Intersection
    BitVector meetAvail(BitVector bv1, BitVector bv2) { bv1 &= bv2; return bv1; }

    // Transfer function: OUT = (IN - KILL) U GEN
    BitVector transferAvail(BasicBlock* B, const BitVector& InSet) {
      BitVector OutSet = InSet;
      auto kill_it = killSets.find(B);
      auto gen_it = genSets.find(B);
      if (kill_it != killSets.end()) { OutSet.reset(kill_it->second); /* OUT = IN - KILL */ }
      if (gen_it != genSets.end()) { OutSet |= gen_it->second; /* OUT = OUT U GEN */ }
      return OutSet;
     }

    // Run method for the new Pass Manager
    Result run(Function &F, FunctionAnalysisManager &AM) {
        buildExpressionDomain(F); // Build map/vector of expressions
        if (numExpr > 0) {
            calculateGenKillSets(F); // Calculate GEN/KILL for all blocks

            // Configure and run the dataflow analysis
            df.initializeDomain(numExpr);
            BitVector intersectionIdentity(numExpr, true); // Identity for intersection is 'all true'
            df.setDirection(Dataflow::FORWARD)
              .setBoundary(Dataflow::EMPTY) // Nothing available at the very start
              .setInitial(Dataflow::ALL)    // Converges faster if we assume all available initially
              .setMeetOp([this](auto... args) { return this->meetAvail(args...); })
              .setTransferFn([this](auto* b, auto const& inSet) { return this->transferAvail(b, inSet); })
              .setMeetIdentity(intersectionIdentity);
            df.run(F, "AvailableExpressions"); // Run the framework

            // *** ADDED: Print results after analysis ***
            printDataflowResults(F);
        }
        // Return 'this' analysis object by moving it
        return std::move(*this);
    }
};
AnalysisKey AvailableExpressions::Key;


//-----------------------------------------------------------------------------
// 2) Anticipated Expressions Analysis Pass (New PM)
//-----------------------------------------------------------------------------
class AnticipatedExpressions : public AnalysisPassBase, public AnalysisInfoMixin<AnticipatedExpressions> {
public:
    friend AnalysisInfoMixin<AnticipatedExpressions>;
    static AnalysisKey Key;
    using Result = AnticipatedExpressions;

    AnticipatedExpressions() = default;

    // Implement GEN/KILL for Anticipated Expressions (Backward)
    void calculateGenKillSets(Function &F) override {
      genSets.clear(); killSets.clear();
      for (auto &BB : F) {
          BitVector gen_b(numExpr); BitVector kill_b(numExpr);
          // Iterate backwards through instructions in the block
          for (auto it = BB.rbegin(), et = BB.rend(); it != et; ++it) {
              Instruction &I = *it;

              // Check if I kills an expression (defines an operand)
              Value* definedValue = nullptr;
              if (!I.getType()->isVoidTy() && !isa<StoreInst>(&I) && !I.isTerminator() && !isa<PHINode>(&I) && !isa<CmpInst>(&I)) {
                  definedValue = &I;
              }
              if (definedValue) {
                  for(size_t i = 0; i < exprVec.size(); ++i) {
                       if (i < exprVec.size() && exprVec[i].isValid()) {
                          if (exprVec[i].v1 == definedValue || exprVec[i].v2 == definedValue) {
                              kill_b.set(i);  // Mark expression as killed
                              gen_b.reset(i); // If killed, it cannot be generated later (backward)
                          }
                       }
                  }
              }

               // Check if I generates an expression (computes it)
               if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
                  Expression currentExpr(&I); if (!currentExpr.isValid()) continue;
                  auto expr_it = exprMap.find(currentExpr);
                  if (expr_it != exprMap.end()) {
                      int idx = expr_it->second;
                      // Generate only if not killed earlier in the backward pass through the block
                      if (!kill_b.test(idx)) {
                          gen_b.set(idx);
                      }
                  }
              }
          }
          genSets[&BB] = gen_b; killSets[&BB] = kill_b;
      }
     }

    // Meet operator: Intersection
    BitVector meetAnticip(BitVector bv1, BitVector bv2) { bv1 &= bv2; return bv1; }

    // Transfer function (Backward): IN = (OUT - KILL) U GEN
    BitVector transferAnticip(BasicBlock* B, const BitVector& OutSet) {
      BitVector InSet = OutSet;
      auto kill_it = killSets.find(B);
      auto gen_it = genSets.find(B);
      if(kill_it != killSets.end()) { InSet.reset(kill_it->second); /* IN = OUT - KILL */ }
      if(gen_it != genSets.end()) { InSet |= gen_it->second; /* IN = IN U GEN */ }
      return InSet;
     }

    // Run method for the new Pass Manager
    Result run(Function &F, FunctionAnalysisManager &AM) {
        buildExpressionDomain(F);
        if (numExpr > 0) {
            calculateGenKillSets(F);

            df.initializeDomain(numExpr);
            BitVector intersectionIdentity(numExpr, true); // Identity for intersection
            df.setDirection(Dataflow::BACKWARD)
              .setBoundary(Dataflow::EMPTY) // Nothing anticipated after the last instruction
              .setInitial(Dataflow::ALL)    // Assume all anticipated initially (converges faster)
              .setMeetOp([this](auto... args) { return this->meetAnticip(args...); })
              .setTransferFn([this](auto* b, auto const& outSet) { return this->transferAnticip(b, outSet); })
              .setMeetIdentity(intersectionIdentity);
            df.run(F, "AnticipatedExpressions");

            // *** ADDED: Print results after analysis ***
            printDataflowResults(F);
        }
        return std::move(*this);
    }
};
AnalysisKey AnticipatedExpressions::Key;


//-----------------------------------------------------------------------------
// 3) Used Expressions Analysis Pass (New PM)
//-----------------------------------------------------------------------------
class UsedExpressions : public AnalysisPassBase, public AnalysisInfoMixin<UsedExpressions> {
public:
    friend AnalysisInfoMixin<UsedExpressions>;
    static AnalysisKey Key;
    using Result = UsedExpressions;
    // Map from the Instruction* that defines an expr -> index in exprVec
    std::map<Value*, int> definingInstToExprIndex;

    UsedExpressions() = default;

    // Implement GEN/KILL for Used Expressions (Backward)
    void calculateGenKillSets(Function &F) override {
        genSets.clear(); killSets.clear();
        // Precompute map from defining instruction to expression index
        this->definingInstToExprIndex.clear();
        for(size_t i = 0; i < exprVec.size(); ++i) {
            if (exprVec[i].isValid() && exprVec[i].definingInst) {
                this->definingInstToExprIndex[exprVec[i].definingInst] = i;
            }
        }

        for (auto &BB : F) {
            BitVector gen_b(numExpr); BitVector kill_b(numExpr);
            // Iterate backwards
            for (auto it = BB.rbegin(), et = BB.rend(); it != et; ++it) {
                Instruction &I = *it;

                // KILL: If I computes expression E, then E is killed (defined here)
                if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
                    Expression currentExpr(&I);
                    if (currentExpr.isValid()) {
                        auto expr_it = exprMap.find(currentExpr);
                        if (expr_it != exprMap.end()) {
                            int idx = expr_it->second;
                            kill_b.set(idx); // Definition kills the expression
                            gen_b.reset(idx); // Cannot be generated if defined here first (backward)
                        }
                    }
                }

                // GEN: If I uses the result of expression E, then E is generated (used here)
                for (unsigned opIdx = 0; opIdx < I.getNumOperands(); ++opIdx) {
                    Value *operand = I.getOperand(opIdx);
                    // Check if this operand is an instruction that defines an expression we track
                    auto definingInstIt = this->definingInstToExprIndex.find(operand);
                    if (definingInstIt != this->definingInstToExprIndex.end()) {
                        int exprIdx = definingInstIt->second;
                        // Generate (mark as used) only if not killed earlier (backward) in this block
                        if (!kill_b.test(exprIdx)) {
                            gen_b.set(exprIdx);
                        }
                    }
                }
            }
            genSets[&BB] = gen_b; killSets[&BB] = kill_b;
        }
     }


    // Meet operator: Union
    BitVector meetUsed(BitVector bv1, BitVector bv2) { bv1 |= bv2; return bv1; }

    // Transfer function (Backward): IN = (OUT - KILL) U GEN
    BitVector transferUsed(BasicBlock* B, const BitVector& OutSet) {
      BitVector InSet = OutSet;
      auto kill_it = killSets.find(B);
      auto gen_it = genSets.find(B);
      if(kill_it != killSets.end()) { InSet.reset(kill_it->second); /* IN = OUT - KILL */ }
      if(gen_it != genSets.end()) { InSet |= gen_it->second; /* IN = IN U GEN */ }
      return InSet;
     }

   // Run method for the new Pass Manager
   Result run(Function &F, FunctionAnalysisManager &AM) {
        buildExpressionDomain(F);
        if (numExpr > 0) {
            calculateGenKillSets(F);

            df.initializeDomain(numExpr);
            BitVector unionIdentity(numExpr, false); // Identity for union is 'all false'
            df.setDirection(Dataflow::BACKWARD)
              .setBoundary(Dataflow::EMPTY) // Nothing used after the last instruction
              .setInitial(Dataflow::EMPTY)  // Assume nothing used initially
              .setMeetOp([this](auto... args) { return this->meetUsed(args...); })
              .setTransferFn([this](auto* b, auto const& outSet) { return this->transferUsed(b, outSet); })
              .setMeetIdentity(unionIdentity);
            df.run(F, "UsedExpressions");

            // *** ADDED: Print results after analysis ***
            printDataflowResults(F);
        }
        return std::move(*this);
   }
};
AnalysisKey UsedExpressions::Key;

//-----------------------------------------------------------------------------
// 4) Postponable Expressions Analysis Pass (New PM) - ADDED
//-----------------------------------------------------------------------------
class PostponableExpressions : public AnalysisPassBase, public AnalysisInfoMixin<PostponableExpressions> {
public:
    friend AnalysisInfoMixin<PostponableExpressions>;
    static AnalysisKey Key;
    using Result = PostponableExpressions;

    PostponableExpressions() = default;

    // Calculate GEN sets for Postponable expressions. KILL depends on UsedExpressions.
    void calculateGenKillSets(Function &F) override {
        genSets.clear();
        killSets.clear(); // Kill sets are not pre-calculated here, determined by Used_IN.

        for (auto &BB : F) {
            BitVector gen_b(numExpr);
            for (auto &I : BB) {
                // GEN = expressions computed in this block
                if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
                    Expression currentExpr(&I);
                    if (!currentExpr.isValid()) continue;
                    auto it = exprMap.find(currentExpr);
                    if (it != exprMap.end()) {
                        gen_b.set(it->second); // Generate expression computed here
                    }
                }
            }
            genSets[&BB] = gen_b;
            // killSets[&BB] is intentionally left empty or not set
        }
    }

    // Meet operator: Union
    BitVector meetPostpon(BitVector bv1, BitVector bv2) {
        bv1 |= bv2;
        return bv1;
    }

    // Transfer function depends on UsedExpressions result (passed via lambda capture)
    // IN = (OUT - KILL) U GEN, where KILL = USED_IN[B]
    BitVector transferPostpon(BasicBlock* B, const BitVector& OutSet, const UsedExpressions::Result& usedResult) {
        BitVector InSet = OutSet;

        // KILL = USED_IN[B] from the UsedExpressions result
        const BitVector& killSet = usedResult.df.getState(B).In; // Get USED_IN for B

        // GEN = computed in B (pre-calculated in genSets)
        const BitVector& genSet = this->genSets.count(B) ? this->genSets.lookup(B) : BitVector(this->numExpr); // Default empty

        // Apply transfer: IN = (OutSet & ~KILL) | GEN
        BitVector notKill = killSet;
        notKill.flip();    // Calculate ~KILL
        InSet &= notKill;  // InSet = OutSet & ~KILL
        InSet |= genSet;   // InSet = InSet | GEN

        return InSet;
    }

    // Run method for the new Pass Manager
    Result run(Function &F, FunctionAnalysisManager &AM) {
        // Get the results of the prerequisite UsedExpressions analysis
        auto &usedResult = AM.getResult<UsedExpressions>(F);

        // Copy domain info from UsedExpressions (or rebuild if necessary)
        // Need exprMap and exprVec to be populated.
        buildExpressionDomain(F); // Ensure domain is built for this instance

        if (numExpr > 0) {
            // Calculate GEN sets (KILL is handled in transfer function)
            calculateGenKillSets(F);

            df.initializeDomain(numExpr);
            BitVector unionIdentity(numExpr, false); // Identity for union

            df.setDirection(Dataflow::BACKWARD)
              .setBoundary(Dataflow::EMPTY) // Nothing postponable after the exit
              .setInitial(Dataflow::EMPTY)  // Assume nothing postponable initially
              .setMeetOp([this](auto... args) { return this->meetPostpon(args...); })
              // Pass UsedExpressions result to the transfer function via lambda capture
              .setTransferFn([&usedResult, this](auto* b, auto const& outSet) {
                  return this->transferPostpon(b, outSet, usedResult);
               })
              .setMeetIdentity(unionIdentity);

            df.run(F, "PostponableExpressions");

            // Print the results using the base class method
            printDataflowResults(F);
        }
        return std::move(*this);
    }
};
AnalysisKey PostponableExpressions::Key;


//-----------------------------------------------------------------------------
// 5) Lazy Code Motion Pass (New PM Structure)
//-----------------------------------------------------------------------------
class LazyCodeMotion : public PassInfoMixin<LazyCodeMotion> {

// *** ADDED: Explicit Move Constructor and Assignment Operator ***
public:
    // Default constructor
    LazyCodeMotion() = default;

    // Explicitly define Move Constructor
    LazyCodeMotion(LazyCodeMotion&& Other) noexcept :
        // Move movable members
        exprMap(std::move(Other.exprMap)),
        exprVec(std::move(Other.exprVec)),
        numExpr(Other.numExpr),
        earliestSets(std::move(Other.earliestSets)),
        latest_inSets(std::move(Other.latest_inSets)),
        insertSets(std::move(Other.insertSets)),
        insertedTempsMap(std::move(Other.insertedTempsMap)),
        // replacementMap(std::move(Other.replacementMap)), // Cannot move ValueMap
        originalInstructions(std::move(Other.originalInstructions))
        // Note: replacementMap is default-constructed in the new object.
        // The moved-from object's map is left as is (but the object is typically discarded post-move).
    {
        // Ensure moved-from object is in a valid, safe state (optional but good practice)
        Other.numExpr = 0;
        // Other.replacementMap.clear(); // If necessary
        // Other.exprMap.clear(); // etc.
    }

    // Explicitly define Move Assignment Operator
    LazyCodeMotion& operator=(LazyCodeMotion&& Other) noexcept {
        if (this != &Other) {
            // Move movable members
            exprMap = std::move(Other.exprMap);
            exprVec = std::move(Other.exprVec);
            numExpr = Other.numExpr;
            earliestSets = std::move(Other.earliestSets);
            latest_inSets = std::move(Other.latest_inSets);
            insertSets = std::move(Other.insertSets);
            insertedTempsMap = std::move(Other.insertedTempsMap);
            originalInstructions = std::move(Other.originalInstructions);

            // Handle non-movable ValueMap: Clear current map, leave source map as is.
            replacementMap.clear();

            // Ensure moved-from object is in a valid, safe state (optional)
            Other.numExpr = 0;
            // Other.replacementMap.clear(); // If necessary
            // Other.exprMap.clear(); // etc.
        }
        return *this;
    }

    // Prevent copying
    LazyCodeMotion(const LazyCodeMotion&) = delete;
    LazyCodeMotion& operator=(const LazyCodeMotion&) = delete;

    // Main run method for the new Pass Manager
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);

    // Required by PassInfoMixin, indicates this pass is usable
    static bool isRequired() { return true; }

// --- Member variables and private helper functions ---
private:
    // Domain info (copied from analysis results)
    std::map<Expression, int> exprMap;
    std::vector<Expression> exprVec;
    unsigned numExpr = 0;

    // Sets calculated during LCM
    DenseMap<BasicBlock*, BitVector> earliestSets;
    DenseMap<BasicBlock*, BitVector> latest_inSets;
    DenseMap<BasicBlock*, BitVector> insertSets;

    // --- State for transformation phases (REVISED) ---
    // Map from Block -> Expression -> Inserted Temporary Instruction
    DenseMap<BasicBlock*, DenseMap<Expression, Instruction*, DenseMapInfo<Expression>>> insertedTempsMap;
    // Map from Original Instruction -> Best Replacement Value (resolved temporary)
    ValueMap<Instruction*, Value*> replacementMap; // <-- NON-MOVABLE MEMBER
    // Keep track of original instructions that might become redundant
    DenseSet<Instruction*> originalInstructions;


    // Helper to find the highest dominating temporary insertion point
    Instruction* findHighestDominatingTemp(Instruction* userInst, const Expression& expr, DominatorTree& DT) {
        Instruction* bestTemp = nullptr;
        BasicBlock* userBlock = userInst->getParent();

        for (auto const& [insertBlock, exprToTempMap] : insertedTempsMap) {
            if (!exprToTempMap.count(expr)) continue; // Skip blocks where this expr wasn't inserted

            Instruction* currentTemp = exprToTempMap.lookup(expr);

            // Check if currentTemp dominates the user instruction
            // Need to handle the case where the temp and user are in the same block correctly.
            // DT.dominates(A, B) is true if A == B.
            // If they are the same instruction, temp doesn't dominate in the sense we need.
            if (currentTemp == userInst) continue;

            if (DT.dominates(currentTemp, userInst)) {
                if (!bestTemp) {
                    // First dominating temp found
                    bestTemp = currentTemp;
                } else {
                    // Check if currentTemp's block dominates bestTemp's block
                    // If it does, currentTemp is "higher" in the dominator tree.
                    // If they are in the same block, prefer the one defined earlier (closer to the start).
                    // Since we insert at the start, this dominance check is okay.
                    if (DT.dominates(insertBlock, bestTemp->getParent())) {
                         bestTemp = currentTemp;
                    }
                 }
            }
        }
         return bestTemp;
    }

     // Helper to resolve replacement chains
    Value* resolveReplacement(Value* V, ValueMap<Instruction*, Value*>& currentReplacements) {
        Value* current = V;
        // Limit iterations to prevent infinite loops in case of cycles (shouldn't happen with dominance)
        for (int i = 0; i < 100; ++i) { // Increased limit slightly
            if (auto* Inst = dyn_cast<Instruction>(current)) {
                 auto it = currentReplacements.find(Inst);
                 if (it != currentReplacements.end()) {
                    // Ensure we don't cycle (e.g., A->B and B->A)
                    if (it->second == V) {
                        errs() << "Warning: Replacement cycle detected involving: "; Inst->print(errs()); errs() << "\n";
                        return Inst; // Return original instruction to break cycle
                    }
                    current = it->second;
                 } else {
                    break; // No further replacement for this instruction
                 }
             } else {
                break; // Not an instruction, cannot be in the replacement map key
             }
         }
        return current;
     }


    // Build the domain of expressions for the given function (Internal helper)
    void buildExpressionDomain(Function &F) {
        exprMap.clear(); exprVec.clear(); numExpr = 0; int idx = 0;
        for (auto &BB : F) { for (auto &I : BB) { if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
            Expression expr(&I); if (!expr.isValid()) continue;
            auto result = exprMap.insert({expr, idx});
            if (result.second) { exprVec.push_back(expr); idx++; }
        }}}
        numExpr = exprVec.size();
     }

    // Optional: Print helper (Internal helper)
    void printSetMap(StringRef setName, Function &F, const DenseMap<BasicBlock*, BitVector>& setMap) {
        outs() << "\n--- " << setName << " Sets ---\n"; outs().flush();
        for (auto &BB : F) {
            BasicBlock* B = &BB;
             outs() << "Basic Block ";
            if (B->hasName()) outs() << B->getName(); else outs() << "<bb " << (void*)B << ">";
            if (setMap.count(B)) {
                if (!this->exprVec.empty()) {
                    outs() << ": " << Dataflow::bitVectorExprToString(setMap.lookup(B), this->exprVec) << "\n";
                } else { outs() << ": <ExprVec empty>\n"; }
            }
            else { outs() << ": <Not computed>\n"; }
        } outs() << "--------------------\n"; outs().flush();
     }

}; // End of LazyCodeMotion class definition


// =============================================================================
// Implementation of the new PM run method for LazyCodeMotion (REVISED)
// =============================================================================
PreservedAnalyses LazyCodeMotion::run(Function &F, FunctionAnalysisManager &AM) {
    // *** ADD THIS FLAG ***
    // Set to true for LCM-E (insert based on earliest), false for LCM-L (insert based on insertSets)
    // --> CHANGE THIS VALUE TO 'true' TO RUN IN EARLIEST MODE <--
  bool useEarliestInsertion = false; // Default to LCM-L (like original behavior)
 //bool useEarliestInsertion = true;    // Default to LCM-E

    bool Changed = false; // Track if the IR is modified

    // --- Clear state ---
    insertedTempsMap.clear();
    replacementMap.clear();
    originalInstructions.clear();
    earliestSets.clear();
    latest_inSets.clear();
    insertSets.clear();

    // --- Get prerequisite analysis results ---
    auto &AvailResult = AM.getResult<AvailableExpressions>(F);
    auto &AnticResult = AM.getResult<AnticipatedExpressions>(F);
    auto &UsedResult = AM.getResult<UsedExpressions>(F);
    auto &DT = AM.getResult<DominatorTreeAnalysis>(F); // Get Dominator Tree

    // --- Copy domain info ---
    if (AvailResult.numExpr == 0 || AvailResult.exprVec.empty()) {
        outs() << "LCM: No expressions found or domain empty in function " << F.getName() << ". Skipping.\n";
        return PreservedAnalyses::all();
    }
    exprMap = AvailResult.exprMap;
    exprVec = AvailResult.exprVec;
    numExpr = AvailResult.numExpr;

    // Collect all original binary instructions for later processing
    for(auto& BB : F) {
        for(auto& I : BB) {
            if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
                 Expression e(BO);
                 if (e.isValid()) { originalInstructions.insert(BO); }
             }
         }
     }

    // --- Get dataflow states (IN/OUT sets) ---
    const auto& availStates = AvailResult.df.getStates();
    const auto& anticStates = AnticResult.df.getStates();
    const auto& usedStates = UsedResult.df.getStates();


    // --- Step 1: Calculate EARLIEST[B] = ANTIC_IN[B] & ! (AVAIL_IN[B] | USED_IN[B]) ---
    // Using: EARLIEST[B] = ANTIC_IN[B] & (~AVAIL_IN[B] | USED_IN[B])
    outs() << "LCM: Calculating EARLIEST sets...\n"; outs().flush();
    for (auto &BB : F) {
        BasicBlock* B = &BB; BitVector earliest_b(numExpr);
        auto avail_it = availStates.find(B);
        auto antic_it = anticStates.find(B);
        auto used_it = usedStates.find(B);

        if (avail_it != availStates.end() && antic_it != anticStates.end() && used_it != usedStates.end()) {
            const BitVector& avail_in = avail_it->second.In;
            const BitVector& antic_in = antic_it->second.In;
            const BitVector& used_in = used_it->second.In;

            BitVector not_avail = avail_in; not_avail.flip(); // ~AVAIL_IN
            BitVector not_avail_or_used = not_avail;
            not_avail_or_used |= used_in; // ~AVAIL_IN | USED_IN

            earliest_b = antic_in;
            earliest_b &= not_avail_or_used; // ANTIC_IN & (~AVAIL_IN | USED_IN)

        } else {
             errs() << "Warning: Missing state for block " << (B->hasName() ? B->getName().str() : "<anon>") << " in Earliest calculation.\n";
        }
        earliestSets[B] = earliest_b;
     }
    // printSetMap("EARLIEST", F, earliestSets);


    // --- Step 2: Calculate LATEST_IN[B] (Iterative Dataflow) ---
    // LATEST_IN[B] = (EARLIEST[B] | USED_IN[B]) & meet(LATEST_IN[P]) for P in pred(B)
    outs() << "LCM: Calculating LATEST_IN sets...\n"; outs().flush();
    DenseMap<BasicBlock*, BitVector> current_latest_in;
    for (auto &BB : F) { current_latest_in[&BB] = BitVector(numExpr, true); } // Init to ALL true

    bool latest_changed = true; unsigned latest_iterations = 0;
    const unsigned MAX_LATEST_ITERATIONS = F.size() * 2 + 10; // Adjusted limit

    while (latest_changed && latest_iterations < MAX_LATEST_ITERATIONS) {
        latest_iterations++; latest_changed = false;
        for (auto &BB : F) {
            BasicBlock* B = &BB; BitVector meet_preds(numExpr, true);
            if (pred_begin(B) != pred_end(B)) {
                bool first_pred = true;
                for (BasicBlock* P : predecessors(B)) {
                    auto pred_latest_it = current_latest_in.find(P);
                    if (pred_latest_it != current_latest_in.end()) {
                         if (first_pred) { meet_preds = pred_latest_it->second; first_pred = false; }
                         else { meet_preds &= pred_latest_it->second; }
                    } else { meet_preds.reset(); break; } // Error case
                }
            } // Else: Entry block, meet_preds remains all true

            BitVector new_latest_in_b(numExpr, false);
            auto earliest_it = earliestSets.find(B); auto used_it = usedStates.find(B);
            if (earliest_it != earliestSets.end() && used_it != usedStates.end()) {
                const BitVector& earliest_b_val = earliest_it->second;
                const BitVector& used_in_b = used_it->second.In;
                BitVector earliest_or_used = earliest_b_val; earliest_or_used |= used_in_b;
                new_latest_in_b = earliest_or_used; new_latest_in_b &= meet_preds;
            } else { /* Error */ }

            auto current_it = current_latest_in.find(B);
            if (current_it == current_latest_in.end() || current_it->second != new_latest_in_b) {
                current_latest_in[B] = new_latest_in_b; latest_changed = true;
            }
        } // End block loop
     } // End while loop
    if (latest_iterations >= MAX_LATEST_ITERATIONS) { errs() << "Warning: LATEST_IN calc timeout.\n"; }
    latest_inSets = current_latest_in;
    // printSetMap("LATEST_IN", F, latest_inSets);


    // --- Step 3: Calculate INSERT[B] = LATEST_IN[B] & (EARLIEST[B] | (~LATEST_IN[P] for some P)) ---
    outs() << "LCM: Calculating INSERT sets...\n"; outs().flush();
    for (auto &BB : F) {
        BasicBlock* B = &BB; BitVector insert_b(numExpr, false);
        auto latest_it = latest_inSets.find(B); auto earliest_it = earliestSets.find(B);
        if (latest_it == latest_inSets.end() || earliest_it == earliestSets.end()) { insertSets[B] = insert_b; continue; }

        const BitVector& latest_in_b = latest_it->second;
        const BitVector& earliest_b_val = earliest_it->second;
        BitVector not_latest_in_preds(numExpr, false); // Is true if E is NOT latest_in ALL predecessors
        if (pred_begin(B) != pred_end(B)) {
            for (BasicBlock* P : predecessors(B)) {
                auto pred_latest_it = latest_inSets.find(P);
                if (pred_latest_it != latest_inSets.end()) {
                    BitVector temp = pred_latest_it->second; temp.flip(); // ~LATEST_IN[P]
                    not_latest_in_preds |= temp; // If *any* pred lacks it, set bit
                } else { not_latest_in_preds.set(); break; } // Error: Assume condition met
            }
        } else { not_latest_in_preds.set(); } // Entry block: Condition met

        BitVector insert_condition = earliest_b_val; insert_condition |= not_latest_in_preds;
        insert_b = latest_in_b; insert_b &= insert_condition;
        insertSets[B] = insert_b;
     }
     // printSetMap("INSERT", F, insertSets);


    // --- Phase 1: Insertion ---
    // *** MODIFIED TO SUPPORT E/L SWITCH ***
    outs() << "LCM: Phase 1 - Inserting temporary computations (" << (useEarliestInsertion ? "Earliest Mode" : "Latest Mode") << ")...\n"; outs().flush();
    insertedTempsMap.clear();

    for (auto &BB : F) {
        BasicBlock* B = &BB;

        // *** CHOOSE THE SET BASED ON THE FLAG (REVISED W/ find) ***
        const BitVector* set_to_use = nullptr;
        if (useEarliestInsertion) {
            // In Earliest mode, we insert if the expression is in EARLIEST[B]
            auto it = earliestSets.find(B); // Use find to get iterator
            if (it != earliestSets.end()) {
                set_to_use = &it->second; // Take address of the value in the map
            } else {
                 errs() << "Warning: Missing EARLIEST set for block " << (B->hasName() ? B->getName().str() : "<anon>") << " in E-mode insertion.\n";
                 continue; // Skip block if set missing
            }
        } else { // Use Latest/Insert mode (default LCM-L behavior)
            auto it = insertSets.find(B); // Use find to get iterator
            if (it != insertSets.end()) {
                 set_to_use = &it->second; // Take address of the value in the map
            } else {
                // This might happen if LATEST/INSERT couldn't be computed
                // errs() << "Warning: Missing INSERT set for block " << B->getName() << " in L-mode insertion.\n";
                continue; // Skip block if set missing
            }
        }
        // -----------------------------------------

        // Check if the chosen set is valid and has any bits set
        // (set_to_use will be nullptr if find failed)
        if (!set_to_use || set_to_use->none()) continue;
        const BitVector& insert_or_earliest_b = *set_to_use; // Dereference the pointer
        // -----------------------------------------


        Instruction *insertBefore = B->getFirstNonPHIOrDbgOrLifetime();
        if (!insertBefore) { insertBefore = B->getTerminator(); }
        if (!insertBefore) { errs() << "Warning: No insert point in " << B->getName() <<"\n"; continue; }
        IRBuilder<> builder(insertBefore);
        auto& blockInsertedTemps = insertedTempsMap[B]; // Get/create map for block B

        // *** Iterate using the chosen set (insert_or_earliest_b) ***
        for (int i = insert_or_earliest_b.find_first(); i != -1; i = insert_or_earliest_b.find_next(i)) {
             if (i >= (int)exprVec.size() || i < 0) continue; // Invalid index
             const Expression& e = exprVec[i];
             if (!e.isValid()) continue; // Invalid expression

             // Operand Dominance Check (crucial)
             bool operands_dominate = true;
             if (Instruction* op1_inst = dyn_cast<Instruction>(e.v1)) { if (!DT.dominates(op1_inst, insertBefore)) operands_dominate = false; }
             else if (Argument* op1_arg = dyn_cast<Argument>(e.v1)) { if (op1_arg->getParent() != &F) operands_dominate = false; }
             else if (!isa<Constant>(e.v1)) { operands_dominate = false; } // Be conservative for other Value types
             if (Instruction* op2_inst = dyn_cast<Instruction>(e.v2)) { if (!DT.dominates(op2_inst, insertBefore)) operands_dominate = false; }
             else if (Argument* op2_arg = dyn_cast<Argument>(e.v2)) { if (op2_arg->getParent() != &F) operands_dominate = false; }
             else if (!isa<Constant>(e.v2)) { operands_dominate = false; } // Be conservative

             if (operands_dominate) {
                 Value *newVal = builder.CreateBinOp(e.op, e.v1, e.v2, "lcm.tmp");
                 if (Instruction* newInst = dyn_cast<Instruction>(newVal)) {
                     outs() << "  Inserted: "; newInst->print(outs()); outs() << " into " << (B->hasName() ? B->getName().str() : "<anon>") << "\n";
                     blockInsertedTemps[e] = newInst; // Store in block's map
                     Changed = true;
                 }
             } else {
                 outs() << "  Skipped Insertion (Dominance): " << e.toString() << " in " << (B->hasName() ? B->getName().str() : "<anon>") << "\n";
             }
        } // End loop over expressions
     } // End loop over BasicBlocks
     // *** END OF MODIFIED SECTION FOR E/L SWITCH ***


// *** BEGIN SECTION for Critical Edge Detection (Phase 1.5) ***
    // *** INSTRUMENTED VERSION with Debug Prints ***
    outs() << "LCM: Phase 1.5 - Checking for potential critical edge insertions...\n"; outs().flush();
    for (auto &BB : F) {
        BasicBlock* B = &BB;
        std::string B_name = B->hasName() ? B->getName().str() : "<anon_block>";
        outs() << "  Checking Block: " << B_name << "\n"; // DEBUG

        // Check if B has multiple predecessors (is a merge point)
        auto predIt = pred_begin(B), predEnd = pred_end(B);
        bool hasMultiplePreds = (predIt != predEnd);
        if (hasMultiplePreds) { auto checkIt = predIt; checkIt++; if (checkIt == predEnd) { hasMultiplePreds = false; } } // Check for >1 pred
        outs() << "    Block " << B_name << " has multiple predecessors? " << (hasMultiplePreds ? "Yes" : "No") << "\n"; // DEBUG

        if (hasMultiplePreds) {
            // Get AVAIL_IN[B] and ANTIC_IN[B]
            auto avail_in_it = availStates.find(B);
            auto antic_in_it = anticStates.find(B);

            if (avail_in_it == availStates.end() || antic_in_it == anticStates.end()) {
                 errs() << "Warning: Missing AVAIL_IN or ANTIC_IN state for multi-pred block " << B_name << " during critical edge check.\n";
                 continue;
            }
            const BitVector& avail_in_b = avail_in_it->second.In;
            const BitVector& antic_in_b = antic_in_it->second.In;
            outs() << "    AVAIL_IN : " << Dataflow::bitVectorExprToString(avail_in_b, exprVec) << "\n"; // DEBUG
            outs() << "    ANTIC_IN : " << Dataflow::bitVectorExprToString(antic_in_b, exprVec) << "\n"; // DEBUG

            // Iterate through all expressions
            for(int i = 0; i < numExpr; ++i) {
                const Expression& e = exprVec[i];
                if (!e.isValid()) continue;
                outs() << "      Checking Expr " << i << " [" << e.toString() << "]: ANTIC=" << antic_in_b[i] << ", AVAIL=" << avail_in_b[i] << "\n"; // DEBUG

                // Condition: Expression is ANTICIPATED at B's entry
                // AND it wasn't already available right at the start of B (AVAIL_IN)
                if (antic_in_b[i] && !avail_in_b[i]) {
                    outs() << "        Heuristic PASSED for Expr " << i << " [" << e.toString() << "]\n"; // DEBUG

                    predIt = pred_begin(B); // Reset predecessor iterator before the inner loop
                    bool reportedForThisExpr = false;

                    // Now check if any incoming edge P->B is critical
                    outs() << "        Checking Predecessors:\n"; // DEBUG
                    for (; predIt != predEnd; ++predIt) {
                        BasicBlock* P = *predIt;
                        std::string P_name = P->hasName() ? P->getName().str() : "<anon_pred>";
                        outs() << "          Pred P: " << P_name << "\n"; // DEBUG

                        // Check if P has multiple successors
                        auto succIt = succ_begin(P), succEnd = succ_end(P);
                        bool hasMultipleSuccs = (succIt != succEnd);
                         if (hasMultipleSuccs) { auto checkIt = succIt; checkIt++; if (checkIt == succEnd) { hasMultipleSuccs = false; } } // Check for >1 succ
                        outs() << "            Pred P has multiple successors? " << (hasMultipleSuccs ? "Yes" : "No") << "\n"; // DEBUG

                        if (hasMultipleSuccs) {
                             // Edge P->B is critical because P has multiple successors AND B has multiple predecessors.
                            outs() << "            --> Critical Edge Detected: " << P_name << " -> " << B_name << "\n"; // DEBUG
                            outs() << "              Info: Expression '" << e.toString()
                                   << "' might benefit from insertion on critical edge: "
                                   << P_name << " -> " << B_name
                                   << " (Code duplication/splitting not implemented).\n"; // Original Info Message
                            reportedForThisExpr = true;
                            // Optional: Break after finding the first critical edge for this expression
                            // if (reportedForThisExpr) break;
                        }
                    } // end loop through predecessors P
                } // end if (antic_in && !avail_in)
            } // end loop through expressions i
        } // end if (B has multiple predecessors)
        outs() << "  Finished Checking Block: " << B_name << "\n"; // DEBUG
    } // end loop through BasicBlocks BB
    outs() << "LCM: Phase 1.5 - Finished Checking.\n"; outs().flush(); // DEBUG
    // *** END SECTION for Critical Edge Detection (Phase 1.5) ***



    // --- Phase 2: Build Replacement Map (REVISED) ---
    // For each original instruction, find the highest dominating temporary to replace it with.
    outs() << "LCM: Phase 2 - Build Replacement Map...\n"; outs().flush();
    replacementMap.clear(); // Clear map before building

    for (Instruction* I : originalInstructions) {
         if (auto *BO = dyn_cast<BinaryOperator>(I)) {
             Expression currentExpr(BO);
             if (!currentExpr.isValid()) continue;

             // Find the highest dominating temp for this expression and instruction location
             Instruction* bestReplacement = findHighestDominatingTemp(BO, currentExpr, DT);

             if (bestReplacement && bestReplacement != BO) {
                 outs() << "  Marking replacement: "; BO->print(outs()); outs() << " -> "; bestReplacement->print(outs()); outs() << "\n";
                 replacementMap[BO] = bestReplacement; // Map original inst to the chosen temp
                 // Changed = true; // Marking doesn't change IR yet
             }
         }
     }


    // --- Phase 3: Perform Replacements and Deletions (REVISED) ---
    outs() << "LCM: Phase 3 - Perform Replacements and Deletions...\n"; outs().flush();

    // Iteratively replace uses until no more changes, handling chains
    bool replacementOccurred;
    int replacementIterations = 0;
    const int MAX_REPLACE_ITER = 10; // Limit iterations

    do {
        replacementOccurred = false;
        replacementIterations++;

        // Need a copy because we might modify instructions during iteration
        std::vector<Instruction*> instructionsToProcess;
        for(auto& BB : F) {
            for(auto& I : BB) {
                instructionsToProcess.push_back(&I);
            }
        }

        for(Instruction* userInst : instructionsToProcess) {
             // Iterate through operands of the user instruction
             for (unsigned i = 0; i < userInst->getNumOperands(); ++i) {
                Value *operand = userInst->getOperand(i);
                if (auto *operandInst = dyn_cast<Instruction>(operand)) {
                    // Check if this operand instruction should be replaced
                    // Resolve potential replacement chains
                    Value* ultimateReplacement = resolveReplacement(operandInst, replacementMap);

                     // If the ultimate replacement is different from the current operand
                     if (ultimateReplacement && ultimateReplacement != operand) {
                         // Dominance check for replacement
                         bool canReplace = false;
                         if (isa<PHINode>(userInst)) {
                             // For PHI nodes, the value must dominate the end of the corresponding predecessor block
                             PHINode *PN = cast<PHINode>(userInst);
                             BasicBlock *predBlock = PN->getIncomingBlock(i);
                              // Check if the replacement dominates the predecessor's terminator
                              if (Instruction* replInst = dyn_cast<Instruction>(ultimateReplacement)) {
                                   // Use properlyDominates for stricter check if needed, but dominates should suffice
                                   if (DT.dominates(replInst, predBlock->getTerminator())) {
                                       canReplace = true;
                                   }
                              } else if (isa<Argument>(ultimateReplacement) || isa<Constant>(ultimateReplacement)) {
                                   canReplace = true; // Constants/Args always dominate
                              }
                          } else {
                              // For non-PHI nodes, the value must dominate the user instruction itself
                              if (Instruction* replInst = dyn_cast<Instruction>(ultimateReplacement)) {
                                  if (DT.dominates(replInst, userInst)) {
                                       canReplace = true;
                                  }
                              } else if (isa<Argument>(ultimateReplacement) || isa<Constant>(ultimateReplacement)) {
                                  canReplace = true;
                              }
                         }

                         if (canReplace) {
                              outs() << "  Replacing operand " << i << " of: "; userInst->print(outs()); outs() << " ("; operand->printAsOperand(outs(), false); outs() << " -> "; ultimateReplacement->printAsOperand(outs(), false); outs() << ")\n";
                              userInst->setOperand(i, ultimateReplacement);
                              replacementOccurred = true;
                              Changed = true;
                         } else {
                              // This warning can be noisy if dominance is the intended reason for not replacing
                              // outs() << "  Skipped replacement (Dominance fail) in: "; userInst->print(outs()); outs() << " operand "; operand->printAsOperand(outs(), false); outs() << " with "; ultimateReplacement->printAsOperand(outs(),false); outs() << "\n";
                         }
                    }
                 }
             } // End operand loop
        } // End instruction loop

    } while (replacementOccurred && replacementIterations < MAX_REPLACE_ITER);

    if (replacementIterations >= MAX_REPLACE_ITER) {
        errs() << "Warning: Replacement phase reached iteration limit.\n";
    }


    // --- Deletion Phase ---
    // Delete original instructions that were replaced AND are now trivially dead
    outs() << "  Deleting dead instructions...\n";
    unsigned deletedCount = 0;
    // Use LLVM's utility to remove trivially dead instructions, which handles dependencies better
    // Iterate backwards to potentially make it easier to delete instructions that use other deleted ones
    SmallVector<Instruction*, 32> toDelete; // Collect instructions to delete
    for(Instruction* originalInst : originalInstructions) {
        // Check if this instruction was actually replaced (is in the map)
        if (replacementMap.count(originalInst)) {
             // Check if it's trivially dead AFTER replacements have happened
             if (isInstructionTriviallyDead(originalInst, nullptr)) {
                 toDelete.push_back(originalInst);
             } else if (originalInst->use_empty()){
                 // Sometimes isInstructionTriviallyDead might miss things? Double check use_empty
                 toDelete.push_back(originalInst);
             }
        }
    }

    // Now delete them
    for (Instruction* I : toDelete) {
         outs() << "    Deleting: "; I->print(outs()); outs() << "\n";
         I->eraseFromParent();
         deletedCount++;
         Changed = true; // Deletion changes IR
     }

    // Optional: Run RecursivelyDeleteTriviallyDeadInstructions for broader cleanup of temps etc.
    // bool cleanupChanged = RecursivelyDeleteTriviallyDeadInstructions(F, nullptr);
    // Changed |= cleanupChanged;

    outs() << "  Deleted " << deletedCount << " original instructions.\n";


    // --- Determine Preserved Analyses ---
    if (!Changed) {
        return PreservedAnalyses::all();
    } else {
        // Basic invalidation: Assume CFG might change if blocks become empty,
        // and analyses relying on instruction details are invalid.
        PreservedAnalyses PA = PreservedAnalyses::none();
        // DominatorTree might be preserved if no blocks were removed/added,
        // but let's be conservative and invalidate it unless we use SplitCriticalEdge utils.
        // PA.preserve<DominatorTreeAnalysis>();
        return PA;
    }
}

// End of LazyCodeMotion::run method implementation

} // end namespace UnifiedPass


//==================== PLUGIN ENTRY POINT ====================//
extern "C" LLVM_ATTRIBUTE_UNUSED LLVM_EXTERNAL_VISIBILITY
PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "UnifiedPass", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
        // Register the analysis passes so the Pass Manager knows about them
        PB.registerAnalysisRegistrationCallback( [](FunctionAnalysisManager &FAM) {
             FAM.registerPass([&] { return UnifiedPass::AvailableExpressions(); });
             FAM.registerPass([&] { return UnifiedPass::AnticipatedExpressions(); });
             FAM.registerPass([&] { return UnifiedPass::UsedExpressions(); });
             FAM.registerPass([&] { return UnifiedPass::PostponableExpressions(); }); // Register Postponable
             FAM.registerPass([&] { return DominatorTreeAnalysis(); }); // Register Dominator Tree
        } );

        // Register the LCM transformation pass and individual print passes
        PB.registerPipelineParsingCallback(
            [](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) -> bool {
                if (Name == "lcm") {
                    // Add the required analysis passes first, then the transformation
                    // LCM doesn't strictly need Postponable, so only require the ones it uses.
                    FPM.addPass(RequireAnalysisPass<UnifiedPass::AvailableExpressions, Function>());
                    FPM.addPass(RequireAnalysisPass<UnifiedPass::AnticipatedExpressions, Function>());
                    FPM.addPass(RequireAnalysisPass<UnifiedPass::UsedExpressions, Function>());
                    FPM.addPass(RequireAnalysisPass<DominatorTreeAnalysis, Function>());
                    // Add the LCM pass itself
                    FPM.addPass(UnifiedPass::LazyCodeMotion());
                    return true; // Name recognized
                }
                 if (Name == "print-avail") {
                     FPM.addPass(RequireAnalysisPass<UnifiedPass::AvailableExpressions, Function>());
                     // The analysis pass itself prints results, no need for extra pass.
                     return true;
                 }
                  if (Name == "print-anticip") {
                     FPM.addPass(RequireAnalysisPass<UnifiedPass::AnticipatedExpressions, Function>());
                     return true;
                 }
                  if (Name == "print-used") {
                     FPM.addPass(RequireAnalysisPass<UnifiedPass::UsedExpressions, Function>());
                     return true;
                 }
                 if (Name == "print-postpon") { // Add print option for Postponable
                     // Postponable requires Used, so make sure Used runs first
                     FPM.addPass(RequireAnalysisPass<UnifiedPass::UsedExpressions, Function>());
                     FPM.addPass(RequireAnalysisPass<UnifiedPass::PostponableExpressions, Function>());
                     return true;
                 }
                return false; // Name not recognized
            }
        );
    }
  };
}
