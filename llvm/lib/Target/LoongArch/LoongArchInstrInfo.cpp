//=- LoongArchInstrInfo.cpp - LoongArch Instruction Information -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the LoongArch implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "LoongArchInstrInfo.h"
#include "LoongArch.h"
#include "LoongArchMachineFunctionInfo.h"
#include "LoongArchRegisterInfo.h"
#include "MCTargetDesc/LoongArchMCTargetDesc.h"
#include "MCTargetDesc/LoongArchMatInt.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/MC/MCInstBuilder.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "LoongArchGenInstrInfo.inc"

LoongArchInstrInfo::LoongArchInstrInfo(LoongArchSubtarget &STI)
    : LoongArchGenInstrInfo(LoongArch::ADJCALLSTACKDOWN,
                            LoongArch::ADJCALLSTACKUP),
      STI(STI) {}

MCInst LoongArchInstrInfo::getNop() const {
  return MCInstBuilder(LoongArch::ANDI)
      .addReg(LoongArch::R0)
      .addReg(LoongArch::R0)
      .addImm(0);
}

void LoongArchInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator MBBI,
                                     const DebugLoc &DL, Register DstReg,
                                     Register SrcReg, bool KillSrc,
                                     bool RenamableDest,
                                     bool RenamableSrc) const {
  if (LoongArch::GPRRegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(LoongArch::OR), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addReg(LoongArch::R0);
    return;
  }

  // VR->VR copies.
  if (LoongArch::LSX128RegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(LoongArch::VORI_B), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addImm(0);
    return;
  }

  // XR->XR copies.
  if (LoongArch::LASX256RegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(LoongArch::XVORI_B), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addImm(0);
    return;
  }

  // GPR->CFR copy.
  if (LoongArch::CFRRegClass.contains(DstReg) &&
      LoongArch::GPRRegClass.contains(SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(LoongArch::MOVGR2CF), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  // CFR->GPR copy.
  if (LoongArch::GPRRegClass.contains(DstReg) &&
      LoongArch::CFRRegClass.contains(SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(LoongArch::MOVCF2GR), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  // CFR->CFR copy.
  if (LoongArch::CFRRegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(LoongArch::PseudoCopyCFR), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }

  // FPR->FPR copies.
  unsigned Opc;
  if (LoongArch::FPR32RegClass.contains(DstReg, SrcReg)) {
    Opc = LoongArch::FMOV_S;
  } else if (LoongArch::FPR64RegClass.contains(DstReg, SrcReg)) {
    Opc = LoongArch::FMOV_D;
  } else if (LoongArch::GPRRegClass.contains(DstReg) &&
             LoongArch::FPR32RegClass.contains(SrcReg)) {
    // FPR32 -> GPR copies
    Opc = LoongArch::MOVFR2GR_S;
  } else if (LoongArch::GPRRegClass.contains(DstReg) &&
             LoongArch::FPR64RegClass.contains(SrcReg)) {
    // FPR64 -> GPR copies
    Opc = LoongArch::MOVFR2GR_D;
  } else {
    // TODO: support other copies.
    llvm_unreachable("Impossible reg-to-reg copy");
  }

  BuildMI(MBB, MBBI, DL, get(Opc), DstReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

void LoongArchInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register SrcReg,
    bool IsKill, int FI, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI, Register VReg,
    MachineInstr::MIFlag Flags) const {
  MachineFunction *MF = MBB.getParent();
  MachineFrameInfo &MFI = MF->getFrameInfo();

  unsigned Opcode;
  if (LoongArch::GPRRegClass.hasSubClassEq(RC))
    Opcode = TRI->getRegSizeInBits(LoongArch::GPRRegClass) == 32
                 ? LoongArch::ST_W
                 : LoongArch::ST_D;
  else if (LoongArch::FPR32RegClass.hasSubClassEq(RC))
    Opcode = LoongArch::FST_S;
  else if (LoongArch::FPR64RegClass.hasSubClassEq(RC))
    Opcode = LoongArch::FST_D;
  else if (LoongArch::LSX128RegClass.hasSubClassEq(RC))
    Opcode = LoongArch::VST;
  else if (LoongArch::LASX256RegClass.hasSubClassEq(RC))
    Opcode = LoongArch::XVST;
  else if (LoongArch::CFRRegClass.hasSubClassEq(RC))
    Opcode = LoongArch::PseudoST_CFR;
  else
    llvm_unreachable("Can't store this register to stack slot");

  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOStore,
      MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

  BuildMI(MBB, I, DebugLoc(), get(Opcode))
      .addReg(SrcReg, getKillRegState(IsKill))
      .addFrameIndex(FI)
      .addImm(0)
      .addMemOperand(MMO);
}

void LoongArchInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register DstReg,
    int FI, const TargetRegisterClass *RC, const TargetRegisterInfo *TRI,
    Register VReg, MachineInstr::MIFlag Flags) const {
  MachineFunction *MF = MBB.getParent();
  MachineFrameInfo &MFI = MF->getFrameInfo();
  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  unsigned Opcode;
  if (LoongArch::GPRRegClass.hasSubClassEq(RC))
    Opcode = TRI->getRegSizeInBits(LoongArch::GPRRegClass) == 32
                 ? LoongArch::LD_W
                 : LoongArch::LD_D;
  else if (LoongArch::FPR32RegClass.hasSubClassEq(RC))
    Opcode = LoongArch::FLD_S;
  else if (LoongArch::FPR64RegClass.hasSubClassEq(RC))
    Opcode = LoongArch::FLD_D;
  else if (LoongArch::LSX128RegClass.hasSubClassEq(RC))
    Opcode = LoongArch::VLD;
  else if (LoongArch::LASX256RegClass.hasSubClassEq(RC))
    Opcode = LoongArch::XVLD;
  else if (LoongArch::CFRRegClass.hasSubClassEq(RC))
    Opcode = LoongArch::PseudoLD_CFR;
  else
    llvm_unreachable("Can't load this register from stack slot");

  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOLoad,
      MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

  BuildMI(MBB, I, DL, get(Opcode), DstReg)
      .addFrameIndex(FI)
      .addImm(0)
      .addMemOperand(MMO);
}

void LoongArchInstrInfo::movImm(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MBBI,
                                const DebugLoc &DL, Register DstReg,
                                uint64_t Val, MachineInstr::MIFlag Flag) const {
  Register SrcReg = LoongArch::R0;

  if (!STI.is64Bit() && !isInt<32>(Val))
    report_fatal_error("Should only materialize 32-bit constants for LA32");

  auto Seq = LoongArchMatInt::generateInstSeq(Val);
  assert(!Seq.empty());

  for (auto &Inst : Seq) {
    switch (Inst.Opc) {
    case LoongArch::LU12I_W:
      BuildMI(MBB, MBBI, DL, get(Inst.Opc), DstReg)
          .addImm(Inst.Imm)
          .setMIFlag(Flag);
      break;
    case LoongArch::ADDI_W:
    case LoongArch::ORI:
    case LoongArch::LU32I_D: // "rj" is needed due to InstrInfo pattern
    case LoongArch::LU52I_D:
      BuildMI(MBB, MBBI, DL, get(Inst.Opc), DstReg)
          .addReg(SrcReg, RegState::Kill)
          .addImm(Inst.Imm)
          .setMIFlag(Flag);
      break;
    case LoongArch::BSTRINS_D:
      BuildMI(MBB, MBBI, DL, get(Inst.Opc), DstReg)
          .addReg(SrcReg, RegState::Kill)
          .addReg(SrcReg, RegState::Kill)
          .addImm(Inst.Imm >> 32)
          .addImm(Inst.Imm & 0xFF)
          .setMIFlag(Flag);
      break;
    default:
      assert(false && "Unknown insn emitted by LoongArchMatInt");
    }

    // Only the first instruction has $zero as its source.
    SrcReg = DstReg;
  }
}

unsigned LoongArchInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  unsigned Opcode = MI.getOpcode();

  if (Opcode == TargetOpcode::INLINEASM ||
      Opcode == TargetOpcode::INLINEASM_BR) {
    const MachineFunction *MF = MI.getParent()->getParent();
    const MCAsmInfo *MAI = MF->getTarget().getMCAsmInfo();
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(), *MAI);
  }

  unsigned NumBytes = 0;
  const MCInstrDesc &Desc = MI.getDesc();

  // Size should be preferably set in
  // llvm/lib/Target/LoongArch/LoongArch*InstrInfo.td (default case).
  // Specific cases handle instructions of variable sizes.
  switch (Desc.getOpcode()) {
  default:
    return Desc.getSize();
  case TargetOpcode::STATEPOINT:
    NumBytes = StatepointOpers(&MI).getNumPatchBytes();
    assert(NumBytes % 4 == 0 && "Invalid number of NOP bytes requested!");
    // No patch bytes means a normal call inst (i.e. `bl`) is emitted.
    if (NumBytes == 0)
      NumBytes = 4;
    break;
  }
  return NumBytes;
}

bool LoongArchInstrInfo::isAsCheapAsAMove(const MachineInstr &MI) const {
  const unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default:
    break;
  case LoongArch::ADDI_D:
  case LoongArch::ORI:
  case LoongArch::XORI:
    return (MI.getOperand(1).isReg() &&
            MI.getOperand(1).getReg() == LoongArch::R0) ||
           (MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0);
  }
  return MI.isAsCheapAsAMove();
}

MachineBasicBlock *
LoongArchInstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  assert(MI.getDesc().isBranch() && "Unexpected opcode!");
  // The branch target is always the last operand.
  return MI.getOperand(MI.getNumExplicitOperands() - 1).getMBB();
}

static void parseCondBranch(MachineInstr &LastInst, MachineBasicBlock *&Target,
                            SmallVectorImpl<MachineOperand> &Cond) {
  // Block ends with fall-through condbranch.
  assert(LastInst.getDesc().isConditionalBranch() &&
         "Unknown conditional branch");
  int NumOp = LastInst.getNumExplicitOperands();
  Target = LastInst.getOperand(NumOp - 1).getMBB();

  Cond.push_back(MachineOperand::CreateImm(LastInst.getOpcode()));
  for (int i = 0; i < NumOp - 1; i++)
    Cond.push_back(LastInst.getOperand(i));
}

bool LoongArchInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                       MachineBasicBlock *&TBB,
                                       MachineBasicBlock *&FBB,
                                       SmallVectorImpl<MachineOperand> &Cond,
                                       bool AllowModify) const {
  TBB = FBB = nullptr;
  Cond.clear();

  // If the block has no terminators, it just falls into the block after it.
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !isUnpredicatedTerminator(*I))
    return false;

  // Count the number of terminators and find the first unconditional or
  // indirect branch.
  MachineBasicBlock::iterator FirstUncondOrIndirectBr = MBB.end();
  int NumTerminators = 0;
  for (auto J = I.getReverse(); J != MBB.rend() && isUnpredicatedTerminator(*J);
       J++) {
    NumTerminators++;
    if (J->getDesc().isUnconditionalBranch() ||
        J->getDesc().isIndirectBranch()) {
      FirstUncondOrIndirectBr = J.getReverse();
    }
  }

  // If AllowModify is true, we can erase any terminators after
  // FirstUncondOrIndirectBR.
  if (AllowModify && FirstUncondOrIndirectBr != MBB.end()) {
    while (std::next(FirstUncondOrIndirectBr) != MBB.end()) {
      std::next(FirstUncondOrIndirectBr)->eraseFromParent();
      NumTerminators--;
    }
    I = FirstUncondOrIndirectBr;
  }

  // Handle a single unconditional branch.
  if (NumTerminators == 1 && I->getDesc().isUnconditionalBranch()) {
    TBB = getBranchDestBlock(*I);
    return false;
  }

  // Handle a single conditional branch.
  if (NumTerminators == 1 && I->getDesc().isConditionalBranch()) {
    parseCondBranch(*I, TBB, Cond);
    return false;
  }

  // Handle a conditional branch followed by an unconditional branch.
  if (NumTerminators == 2 && std::prev(I)->getDesc().isConditionalBranch() &&
      I->getDesc().isUnconditionalBranch()) {
    parseCondBranch(*std::prev(I), TBB, Cond);
    FBB = getBranchDestBlock(*I);
    return false;
  }

  // Otherwise, we can't handle this.
  return true;
}

bool LoongArchInstrInfo::isBranchOffsetInRange(unsigned BranchOp,
                                               int64_t BrOffset) const {
  switch (BranchOp) {
  default:
    llvm_unreachable("Unknown branch instruction!");
  case LoongArch::BEQ:
  case LoongArch::BNE:
  case LoongArch::BLT:
  case LoongArch::BGE:
  case LoongArch::BLTU:
  case LoongArch::BGEU:
    return isInt<18>(BrOffset);
  case LoongArch::BEQZ:
  case LoongArch::BNEZ:
  case LoongArch::BCEQZ:
  case LoongArch::BCNEZ:
    return isInt<23>(BrOffset);
  case LoongArch::B:
  case LoongArch::PseudoBR:
    return isInt<28>(BrOffset);
  }
}

bool LoongArchInstrInfo::isSchedulingBoundary(const MachineInstr &MI,
                                              const MachineBasicBlock *MBB,
                                              const MachineFunction &MF) const {
  if (TargetInstrInfo::isSchedulingBoundary(MI, MBB, MF))
    return true;

  auto MII = MI.getIterator();
  auto MIE = MBB->end();

  // According to psABI v2.30:
  //
  // https://github.com/loongson/la-abi-specs/releases/tag/v2.30
  //
  // The following instruction patterns are prohibited from being reordered:
  //
  // * pcalau12i $a0, %pc_hi20(s)
  //   addi.d $a1, $zero, %pc_lo12(s)
  //   lu32i.d $a1, %pc64_lo20(s)
  //   lu52i.d $a1, $a1, %pc64_hi12(s)
  //
  // * pcalau12i $a0, %got_pc_hi20(s) | %ld_pc_hi20(s) | %gd_pc_hi20(s)
  //   addi.d $a1, $zero, %got_pc_lo12(s)
  //   lu32i.d $a1, %got64_pc_lo20(s)
  //   lu52i.d $a1, $a1, %got64_pc_hi12(s)
  //
  // * pcalau12i $a0, %ie_pc_hi20(s)
  //   addi.d $a1, $zero, %ie_pc_lo12(s)
  //   lu32i.d $a1, %ie64_pc_lo20(s)
  //   lu52i.d $a1, $a1, %ie64_pc_hi12(s)
  //
  // * pcalau12i $a0, %desc_pc_hi20(s)
  //   addi.d $a1, $zero, %desc_pc_lo12(s)
  //   lu32i.d $a1, %desc64_pc_lo20(s)
  //   lu52i.d $a1, $a1, %desc64_pc_hi12(s)
  //
  // For simplicity, only pcalau12i and lu52i.d are marked as scheduling
  // boundaries, and the instructions between them are guaranteed to be
  // ordered according to data dependencies.
  switch (MI.getOpcode()) {
  case LoongArch::PCALAU12I: {
    auto AddI = std::next(MII);
    if (AddI == MIE || AddI->getOpcode() != LoongArch::ADDI_D)
      break;
    auto Lu32I = std::next(AddI);
    if (Lu32I == MIE || Lu32I->getOpcode() != LoongArch::LU32I_D)
      break;
    auto MO0 = MI.getOperand(1).getTargetFlags();
    auto MO1 = AddI->getOperand(2).getTargetFlags();
    auto MO2 = Lu32I->getOperand(2).getTargetFlags();
    if (MO0 == LoongArchII::MO_PCREL_HI && MO1 == LoongArchII::MO_PCREL_LO &&
        MO2 == LoongArchII::MO_PCREL64_LO)
      return true;
    if ((MO0 == LoongArchII::MO_GOT_PC_HI || MO0 == LoongArchII::MO_LD_PC_HI ||
         MO0 == LoongArchII::MO_GD_PC_HI) &&
        MO1 == LoongArchII::MO_GOT_PC_LO && MO2 == LoongArchII::MO_GOT_PC64_LO)
      return true;
    if (MO0 == LoongArchII::MO_IE_PC_HI && MO1 == LoongArchII::MO_IE_PC_LO &&
        MO2 == LoongArchII::MO_IE_PC64_LO)
      return true;
    if (MO0 == LoongArchII::MO_DESC_PC_HI &&
        MO1 == LoongArchII::MO_DESC_PC_LO &&
        MO2 == LoongArchII::MO_DESC64_PC_LO)
      return true;
    break;
  }
  case LoongArch::LU52I_D: {
    auto MO = MI.getOperand(2).getTargetFlags();
    if (MO == LoongArchII::MO_PCREL64_HI || MO == LoongArchII::MO_GOT_PC64_HI ||
        MO == LoongArchII::MO_IE_PC64_HI || MO == LoongArchII::MO_DESC64_PC_HI)
      return true;
    break;
  }
  default:
    break;
  }

  const auto &STI = MF.getSubtarget<LoongArchSubtarget>();
  if (STI.hasFeature(LoongArch::FeatureRelax)) {
    // When linker relaxation enabled, the following instruction patterns are
    // prohibited from being reordered:
    //
    // * pcalau12i $a0, %pc_hi20(s)
    //   addi.w/d $a0, $a0, %pc_lo12(s)
    //
    // * pcalau12i $a0, %got_pc_hi20(s)
    //   ld.w/d $a0, $a0, %got_pc_lo12(s)
    //
    // * pcalau12i $a0, %ld_pc_hi20(s) | %gd_pc_hi20(s)
    //   addi.w/d $a0, $a0, %got_pc_lo12(s)
    //
    // * pcalau12i $a0, %desc_pc_hi20(s)
    //   addi.w/d  $a0, $a0, %desc_pc_lo12(s)
    //   ld.w/d    $ra, $a0, %desc_ld(s)
    //   jirl      $ra, $ra, %desc_call(s)
    unsigned AddiOp = STI.is64Bit() ? LoongArch::ADDI_D : LoongArch::ADDI_W;
    unsigned LdOp = STI.is64Bit() ? LoongArch::LD_D : LoongArch::LD_W;
    switch (MI.getOpcode()) {
    case LoongArch::PCALAU12I: {
      auto MO0 = LoongArchII::getDirectFlags(MI.getOperand(1));
      auto SecondOp = std::next(MII);
      if (MO0 == LoongArchII::MO_DESC_PC_HI) {
        if (SecondOp == MIE || SecondOp->getOpcode() != AddiOp)
          break;
        auto Ld = std::next(SecondOp);
        if (Ld == MIE || Ld->getOpcode() != LdOp)
          break;
        auto MO1 = LoongArchII::getDirectFlags(SecondOp->getOperand(2));
        auto MO2 = LoongArchII::getDirectFlags(Ld->getOperand(2));
        if (MO1 == LoongArchII::MO_DESC_PC_LO && MO2 == LoongArchII::MO_DESC_LD)
          return true;
        break;
      }
      if (SecondOp == MIE ||
          (SecondOp->getOpcode() != AddiOp && SecondOp->getOpcode() != LdOp))
        break;
      auto MO1 = LoongArchII::getDirectFlags(SecondOp->getOperand(2));
      if (MO0 == LoongArchII::MO_PCREL_HI && SecondOp->getOpcode() == AddiOp &&
          MO1 == LoongArchII::MO_PCREL_LO)
        return true;
      if (MO0 == LoongArchII::MO_GOT_PC_HI && SecondOp->getOpcode() == LdOp &&
          MO1 == LoongArchII::MO_GOT_PC_LO)
        return true;
      if ((MO0 == LoongArchII::MO_LD_PC_HI ||
           MO0 == LoongArchII::MO_GD_PC_HI) &&
          SecondOp->getOpcode() == AddiOp && MO1 == LoongArchII::MO_GOT_PC_LO)
        return true;
      break;
    }
    case LoongArch::ADDI_W:
    case LoongArch::ADDI_D: {
      auto MO = LoongArchII::getDirectFlags(MI.getOperand(2));
      if (MO == LoongArchII::MO_PCREL_LO || MO == LoongArchII::MO_GOT_PC_LO)
        return true;
      break;
    }
    case LoongArch::LD_W:
    case LoongArch::LD_D: {
      auto MO = LoongArchII::getDirectFlags(MI.getOperand(2));
      if (MO == LoongArchII::MO_GOT_PC_LO)
        return true;
      break;
    }
    case LoongArch::PseudoDESC_CALL: {
      auto MO = LoongArchII::getDirectFlags(MI.getOperand(2));
      if (MO == LoongArchII::MO_DESC_CALL)
        return true;
      break;
    }
    default:
      break;
    }
  }

  return false;
}

unsigned LoongArchInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                          int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end())
    return 0;

  if (!I->getDesc().isBranch())
    return 0;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();

  I = MBB.end();

  if (I == MBB.begin())
    return 1;
  --I;
  if (!I->getDesc().isConditionalBranch())
    return 1;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();
  return 2;
}

// Inserts a branch into the end of the specific MachineBasicBlock, returning
// the number of instructions inserted.
unsigned LoongArchInstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  if (BytesAdded)
    *BytesAdded = 0;

  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert(Cond.size() <= 3 && Cond.size() != 1 &&
         "LoongArch branch conditions have at most two components!");

  // Unconditional branch.
  if (Cond.empty()) {
    MachineInstr &MI = *BuildMI(&MBB, DL, get(LoongArch::PseudoBR)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(MI);
    return 1;
  }

  // Either a one or two-way conditional branch.
  MachineInstrBuilder MIB = BuildMI(&MBB, DL, get(Cond[0].getImm()));
  for (unsigned i = 1; i < Cond.size(); ++i)
    MIB.add(Cond[i]);
  MIB.addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(*MIB);

  // One-way conditional branch.
  if (!FBB)
    return 1;

  // Two-way conditional branch.
  MachineInstr &MI = *BuildMI(&MBB, DL, get(LoongArch::PseudoBR)).addMBB(FBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(MI);
  return 2;
}

void LoongArchInstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                              MachineBasicBlock &DestBB,
                                              MachineBasicBlock &RestoreBB,
                                              const DebugLoc &DL,
                                              int64_t BrOffset,
                                              RegScavenger *RS) const {
  assert(RS && "RegScavenger required for long branching");
  assert(MBB.empty() &&
         "new block should be inserted for expanding unconditional branch");
  assert(MBB.pred_size() == 1);

  MachineFunction *MF = MBB.getParent();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  const TargetRegisterInfo *TRI = MF->getSubtarget().getRegisterInfo();
  LoongArchMachineFunctionInfo *LAFI =
      MF->getInfo<LoongArchMachineFunctionInfo>();

  if (!isInt<32>(BrOffset))
    report_fatal_error(
        "Branch offsets outside of the signed 32-bit range not supported");

  Register ScratchReg = MRI.createVirtualRegister(&LoongArch::GPRRegClass);
  auto II = MBB.end();

  MachineInstr &PCALAU12I =
      *BuildMI(MBB, II, DL, get(LoongArch::PCALAU12I), ScratchReg)
           .addMBB(&DestBB, LoongArchII::MO_PCREL_HI);
  MachineInstr &ADDI =
      *BuildMI(MBB, II, DL,
               get(STI.is64Bit() ? LoongArch::ADDI_D : LoongArch::ADDI_W),
               ScratchReg)
           .addReg(ScratchReg)
           .addMBB(&DestBB, LoongArchII::MO_PCREL_LO);
  BuildMI(MBB, II, DL, get(LoongArch::PseudoBRIND))
      .addReg(ScratchReg, RegState::Kill)
      .addImm(0);

  RS->enterBasicBlockEnd(MBB);
  Register Scav = RS->scavengeRegisterBackwards(
      LoongArch::GPRRegClass, PCALAU12I.getIterator(), /*RestoreAfter=*/false,
      /*SPAdj=*/0, /*AllowSpill=*/false);
  if (Scav != LoongArch::NoRegister)
    RS->setRegUsed(Scav);
  else {
    // When there is no scavenged register, it needs to specify a register.
    // Specify t8 register because it won't be used too often.
    Scav = LoongArch::R20;
    int FrameIndex = LAFI->getBranchRelaxationSpillFrameIndex();
    if (FrameIndex == -1)
      report_fatal_error("The function size is incorrectly estimated.");
    storeRegToStackSlot(MBB, PCALAU12I, Scav, /*IsKill=*/true, FrameIndex,
                        &LoongArch::GPRRegClass, TRI, Register());
    TRI->eliminateFrameIndex(std::prev(PCALAU12I.getIterator()),
                             /*SpAdj=*/0, /*FIOperandNum=*/1);
    PCALAU12I.getOperand(1).setMBB(&RestoreBB);
    ADDI.getOperand(2).setMBB(&RestoreBB);
    loadRegFromStackSlot(RestoreBB, RestoreBB.end(), Scav, FrameIndex,
                         &LoongArch::GPRRegClass, TRI, Register());
    TRI->eliminateFrameIndex(RestoreBB.back(),
                             /*SpAdj=*/0, /*FIOperandNum=*/1);
  }
  MRI.replaceRegWith(ScratchReg, Scav);
  MRI.clearVirtRegs();
}

static unsigned getOppositeBranchOpc(unsigned Opc) {
  switch (Opc) {
  default:
    llvm_unreachable("Unrecognized conditional branch");
  case LoongArch::BEQ:
    return LoongArch::BNE;
  case LoongArch::BNE:
    return LoongArch::BEQ;
  case LoongArch::BEQZ:
    return LoongArch::BNEZ;
  case LoongArch::BNEZ:
    return LoongArch::BEQZ;
  case LoongArch::BCEQZ:
    return LoongArch::BCNEZ;
  case LoongArch::BCNEZ:
    return LoongArch::BCEQZ;
  case LoongArch::BLT:
    return LoongArch::BGE;
  case LoongArch::BGE:
    return LoongArch::BLT;
  case LoongArch::BLTU:
    return LoongArch::BGEU;
  case LoongArch::BGEU:
    return LoongArch::BLTU;
  }
}

bool LoongArchInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert((Cond.size() && Cond.size() <= 3) && "Invalid branch condition!");
  Cond[0].setImm(getOppositeBranchOpc(Cond[0].getImm()));
  return false;
}

std::pair<unsigned, unsigned>
LoongArchInstrInfo::decomposeMachineOperandsTargetFlags(unsigned TF) const {
  const unsigned Mask = LoongArchII::MO_DIRECT_FLAG_MASK;
  return std::make_pair(TF & Mask, TF & ~Mask);
}

ArrayRef<std::pair<unsigned, const char *>>
LoongArchInstrInfo::getSerializableDirectMachineOperandTargetFlags() const {
  using namespace LoongArchII;
  // TODO: Add more target flags.
  static const std::pair<unsigned, const char *> TargetFlags[] = {
      {MO_CALL, "loongarch-call"},
      {MO_CALL_PLT, "loongarch-call-plt"},
      {MO_PCREL_HI, "loongarch-pcrel-hi"},
      {MO_PCREL_LO, "loongarch-pcrel-lo"},
      {MO_PCREL64_LO, "loongarch-pcrel64-lo"},
      {MO_PCREL64_HI, "loongarch-pcrel64-hi"},
      {MO_GOT_PC_HI, "loongarch-got-pc-hi"},
      {MO_GOT_PC_LO, "loongarch-got-pc-lo"},
      {MO_GOT_PC64_LO, "loongarch-got-pc64-lo"},
      {MO_GOT_PC64_HI, "loongarch-got-pc64-hi"},
      {MO_LE_HI, "loongarch-le-hi"},
      {MO_LE_LO, "loongarch-le-lo"},
      {MO_LE64_LO, "loongarch-le64-lo"},
      {MO_LE64_HI, "loongarch-le64-hi"},
      {MO_IE_PC_HI, "loongarch-ie-pc-hi"},
      {MO_IE_PC_LO, "loongarch-ie-pc-lo"},
      {MO_IE_PC64_LO, "loongarch-ie-pc64-lo"},
      {MO_IE_PC64_HI, "loongarch-ie-pc64-hi"},
      {MO_LD_PC_HI, "loongarch-ld-pc-hi"},
      {MO_GD_PC_HI, "loongarch-gd-pc-hi"},
      {MO_CALL36, "loongarch-call36"},
      {MO_DESC_PC_HI, "loongarch-desc-pc-hi"},
      {MO_DESC_PC_LO, "loongarch-desc-pc-lo"},
      {MO_DESC64_PC_LO, "loongarch-desc64-pc-lo"},
      {MO_DESC64_PC_HI, "loongarch-desc64-pc-hi"},
      {MO_DESC_LD, "loongarch-desc-ld"},
      {MO_DESC_CALL, "loongarch-desc-call"},
      {MO_LE_HI_R, "loongarch-le-hi-r"},
      {MO_LE_ADD_R, "loongarch-le-add-r"},
      {MO_LE_LO_R, "loongarch-le-lo-r"}};
  return ArrayRef(TargetFlags);
}

ArrayRef<std::pair<unsigned, const char *>>
LoongArchInstrInfo::getSerializableBitmaskMachineOperandTargetFlags() const {
  using namespace LoongArchII;
  static const std::pair<unsigned, const char *> TargetFlags[] = {
      {MO_RELAX, "loongarch-relax"}};
  return ArrayRef(TargetFlags);
}

// Returns true if this is the sext.w pattern, addi.w rd, rs, 0.
bool LoongArch::isSEXT_W(const MachineInstr &MI) {
  return MI.getOpcode() == LoongArch::ADDI_W && MI.getOperand(1).isReg() &&
         MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0;
}
