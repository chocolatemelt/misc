//===-- TypeRecord.cpp ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/DebugInfo/CodeView/TypeRecord.h"
#include "llvm/DebugInfo/CodeView/TypeIndex.h"
#include "llvm/DebugInfo/CodeView/RecordSerialization.h"

using namespace llvm;
using namespace llvm::codeview;

//===----------------------------------------------------------------------===//
// Type record deserialization
//===----------------------------------------------------------------------===//

ErrorOr<MemberPointerInfo>
MemberPointerInfo::deserialize(ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  if (auto EC = consumeObject(Data, L))
    return EC;

  TypeIndex T = L->ClassType;
  uint16_t R = L->Representation;
  PointerToMemberRepresentation PMR =
      static_cast<PointerToMemberRepresentation>(R);
  return MemberPointerInfo(T, PMR);
}

ErrorOr<ModifierRecord> ModifierRecord::deserialize(TypeRecordKind Kind,
                                                    ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  if (auto EC = consumeObject(Data, L))
    return EC;

  TypeIndex M = L->ModifiedType;
  uint16_t O = L->Modifiers;
  ModifierOptions MO = static_cast<ModifierOptions>(O);
  return ModifierRecord(M, MO);
}

ErrorOr<ProcedureRecord> ProcedureRecord::deserialize(TypeRecordKind Kind,
                                                      ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  if (auto EC = consumeObject(Data, L))
    return EC;
  return ProcedureRecord(L->ReturnType, L->CallConv, L->Options,
                         L->NumParameters, L->ArgListType);
}

ErrorOr<MemberFunctionRecord>
MemberFunctionRecord::deserialize(TypeRecordKind Kind,
                                  ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  CV_DESERIALIZE(Data, L);
  return MemberFunctionRecord(L->ReturnType, L->ClassType, L->ThisType,
                              L->CallConv, L->Options, L->NumParameters,
                              L->ArgListType, L->ThisAdjustment);
}

ErrorOr<MemberFuncIdRecord>
MemberFuncIdRecord::deserialize(TypeRecordKind Kind, ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  CV_DESERIALIZE(Data, L, Name);
  return MemberFuncIdRecord(L->ClassType, L->FunctionType, Name);
}

ErrorOr<ArgListRecord> ArgListRecord::deserialize(TypeRecordKind Kind,
                                                  ArrayRef<uint8_t> &Data) {
  if (Kind != TypeRecordKind::StringList && Kind != TypeRecordKind::ArgList)
    return std::make_error_code(std::errc::illegal_byte_sequence);

  const Layout *L = nullptr;
  ArrayRef<TypeIndex> Indices;
  CV_DESERIALIZE(Data, L, CV_ARRAY_FIELD_N(Indices, L->NumArgs));
  return ArgListRecord(Kind, Indices);
}

ErrorOr<PointerRecord> PointerRecord::deserialize(TypeRecordKind Kind,
                                                  ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  if (auto EC = consumeObject(Data, L))
    return EC;

  PointerKind PtrKind = L->getPtrKind();
  PointerMode Mode = L->getPtrMode();
  uint32_t Opts = L->Attrs;
  PointerOptions Options = static_cast<PointerOptions>(Opts);
  uint8_t Size = L->getPtrSize();

  if (L->isPointerToMember()) {
    auto E = MemberPointerInfo::deserialize(Data);
    if (E.getError())
      return std::make_error_code(std::errc::illegal_byte_sequence);
    return PointerRecord(L->PointeeType, PtrKind, Mode, Options, Size, *E);
  }

  return PointerRecord(L->PointeeType, PtrKind, Mode, Options, Size);
}

ErrorOr<NestedTypeRecord>
NestedTypeRecord::deserialize(TypeRecordKind Kind, ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  CV_DESERIALIZE(Data, L, Name);
  return NestedTypeRecord(L->Type, Name);
}

ErrorOr<ArrayRecord> ArrayRecord::deserialize(TypeRecordKind Kind,
                                              ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  uint64_t Size;
  StringRef Name;
  CV_DESERIALIZE(Data, L, CV_NUMERIC_FIELD(Size), Name);
  return ArrayRecord(L->ElementType, L->IndexType, Size, Name);
}

ErrorOr<ClassRecord> ClassRecord::deserialize(TypeRecordKind Kind,
                                              ArrayRef<uint8_t> &Data) {
  uint64_t Size = 0;
  StringRef Name;
  StringRef UniqueName;
  uint16_t Props;
  const Layout *L = nullptr;

  CV_DESERIALIZE(Data, L, CV_NUMERIC_FIELD(Size), Name,
                 CV_CONDITIONAL_FIELD(UniqueName, L->hasUniqueName()));

  Props = L->Properties;
  uint16_t WrtValue = (Props & WinRTKindMask) >> WinRTKindShift;
  WindowsRTClassKind WRT = static_cast<WindowsRTClassKind>(WrtValue);
  uint16_t HfaMask = (Props & HfaKindMask) >> HfaKindShift;
  HfaKind Hfa = static_cast<HfaKind>(HfaMask);

  ClassOptions Options = static_cast<ClassOptions>(Props);
  return ClassRecord(Kind, L->MemberCount, Options, Hfa, WRT, L->FieldList,
                     L->DerivedFrom, L->VShape, Size, Name, UniqueName);
}

ErrorOr<UnionRecord> UnionRecord::deserialize(TypeRecordKind Kind,
                                              ArrayRef<uint8_t> &Data) {
  uint64_t Size = 0;
  StringRef Name;
  StringRef UniqueName;
  uint16_t Props;

  const Layout *L = nullptr;
  CV_DESERIALIZE(Data, L, CV_NUMERIC_FIELD(Size), Name,
                 CV_CONDITIONAL_FIELD(UniqueName, L->hasUniqueName()));

  Props = L->Properties;

  uint16_t HfaMask = (Props & HfaKindMask) >> HfaKindShift;
  HfaKind Hfa = static_cast<HfaKind>(HfaMask);
  ClassOptions Options = static_cast<ClassOptions>(Props);
  return UnionRecord(L->MemberCount, Options, Hfa, L->FieldList, Size, Name,
                     UniqueName);
}

ErrorOr<EnumRecord> EnumRecord::deserialize(TypeRecordKind Kind,
                                            ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  StringRef UniqueName;
  CV_DESERIALIZE(Data, L, Name,
                 CV_CONDITIONAL_FIELD(UniqueName, L->hasUniqueName()));

  uint16_t P = L->Properties;
  ClassOptions Options = static_cast<ClassOptions>(P);
  return EnumRecord(L->NumEnumerators, Options, L->FieldListType, Name,
                    UniqueName, L->UnderlyingType);
}

ErrorOr<BitFieldRecord> BitFieldRecord::deserialize(TypeRecordKind Kind,
                                                    ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  CV_DESERIALIZE(Data, L);
  return BitFieldRecord(L->Type, L->BitSize, L->BitOffset);
}

ErrorOr<VFTableShapeRecord>
VFTableShapeRecord::deserialize(TypeRecordKind Kind, ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  if (auto EC = consumeObject(Data, L))
    return EC;

  std::vector<VFTableSlotKind> Slots;
  uint16_t Count = L->VFEntryCount;
  while (Count > 0) {
    if (Data.empty())
      return std::make_error_code(std::errc::illegal_byte_sequence);

    // Process up to 2 nibbles at a time (if there are at least 2 remaining)
    uint8_t Value = Data[0] & 0x0F;
    Slots.push_back(static_cast<VFTableSlotKind>(Value));
    if (--Count > 0) {
      Value = (Data[0] & 0xF0) >> 4;
      Slots.push_back(static_cast<VFTableSlotKind>(Value));
      --Count;
    }
    Data = Data.slice(1);
  }

  return VFTableShapeRecord(Slots);
}

ErrorOr<TypeServer2Record>
TypeServer2Record::deserialize(TypeRecordKind Kind, ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  CV_DESERIALIZE(Data, L, Name);
  return TypeServer2Record(StringRef(L->Guid, 16), L->Age, Name);
}

ErrorOr<StringIdRecord> StringIdRecord::deserialize(TypeRecordKind Kind,
                                                    ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  CV_DESERIALIZE(Data, L, Name);
  return StringIdRecord(L->id, Name);
}

ErrorOr<FuncIdRecord> FuncIdRecord::deserialize(TypeRecordKind Kind,
                                                ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  CV_DESERIALIZE(Data, L, Name);
  return FuncIdRecord(L->ParentScope, L->FunctionType, Name);
}

ErrorOr<UdtSourceLineRecord>
UdtSourceLineRecord::deserialize(TypeRecordKind Kind, ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  CV_DESERIALIZE(Data, L);
  return UdtSourceLineRecord(L->UDT, L->SourceFile, L->LineNumber);
}

ErrorOr<BuildInfoRecord> BuildInfoRecord::deserialize(TypeRecordKind Kind,
                                                      ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  ArrayRef<TypeIndex> Indices;
  CV_DESERIALIZE(Data, L, CV_ARRAY_FIELD_N(Indices, L->NumArgs));
  return BuildInfoRecord(Indices);
}

ErrorOr<VFTableRecord> VFTableRecord::deserialize(TypeRecordKind Kind,
                                                  ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  std::vector<StringRef> Names;
  CV_DESERIALIZE(Data, L, Name, CV_ARRAY_FIELD_TAIL(Names));
  return VFTableRecord(L->CompleteClass, L->OverriddenVFTable, L->VFPtrOffset,
                       Name, Names);
}

ErrorOr<OneMethodRecord> OneMethodRecord::deserialize(TypeRecordKind Kind,
                                                      ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  int32_t VFTableOffset = -1;

  CV_DESERIALIZE(Data, L, CV_CONDITIONAL_FIELD(VFTableOffset,
                                               L->Attrs.isIntroducedVirtual()),
                 Name);

  MethodOptions Options = L->Attrs.getFlags();
  MethodKind MethKind = L->Attrs.getMethodKind();
  MemberAccess Access = L->Attrs.getAccess();
  OneMethodRecord Method(L->Type, MethKind, Options, Access, VFTableOffset,
                         Name);
  // Validate the vftable offset.
  if (Method.isIntroducingVirtual() && Method.getVFTableOffset() < 0)
    return std::make_error_code(std::errc::illegal_byte_sequence);
  return Method;
}

ErrorOr<MethodOverloadListRecord>
MethodOverloadListRecord::deserialize(TypeRecordKind Kind,
                                      ArrayRef<uint8_t> &Data) {
  std::vector<OneMethodRecord> Methods;
  while (!Data.empty()) {
    const Layout *L = nullptr;
    int32_t VFTableOffset = -1;
    CV_DESERIALIZE(Data, L, CV_CONDITIONAL_FIELD(
                                VFTableOffset, L->Attrs.isIntroducedVirtual()));

    MethodOptions Options = L->Attrs.getFlags();
    MethodKind MethKind = L->Attrs.getMethodKind();
    MemberAccess Access = L->Attrs.getAccess();

    Methods.emplace_back(L->Type, MethKind, Options, Access, VFTableOffset,
                         StringRef());

    // Validate the vftable offset.
    auto &Method = Methods.back();
    if (Method.isIntroducingVirtual() && Method.getVFTableOffset() < 0)
      return std::make_error_code(std::errc::illegal_byte_sequence);
  }
  return MethodOverloadListRecord(Methods);
}

ErrorOr<OverloadedMethodRecord>
OverloadedMethodRecord::deserialize(TypeRecordKind Kind,
                                    ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  CV_DESERIALIZE(Data, L, Name);
  return OverloadedMethodRecord(L->MethodCount, L->MethList, Name);
}

ErrorOr<DataMemberRecord>
DataMemberRecord::deserialize(TypeRecordKind Kind, ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  uint64_t Offset;
  StringRef Name;
  CV_DESERIALIZE(Data, L, CV_NUMERIC_FIELD(Offset), Name);
  return DataMemberRecord(L->Attrs.getAccess(), L->Type, Offset, Name);
}

ErrorOr<StaticDataMemberRecord>
StaticDataMemberRecord::deserialize(TypeRecordKind Kind,
                                    ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  StringRef Name;
  CV_DESERIALIZE(Data, L, Name);
  return StaticDataMemberRecord(L->Attrs.getAccess(), L->Type, Name);
}

ErrorOr<EnumeratorRecord>
EnumeratorRecord::deserialize(TypeRecordKind Kind, ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  APSInt Value;
  StringRef Name;
  CV_DESERIALIZE(Data, L, Value, Name);
  return EnumeratorRecord(L->Attrs.getAccess(), Value, Name);
}

ErrorOr<VFPtrRecord> VFPtrRecord::deserialize(TypeRecordKind Kind,
                                              ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  if (auto EC = consumeObject(Data, L))
    return EC;
  return VFPtrRecord(L->Type);
}

ErrorOr<BaseClassRecord> BaseClassRecord::deserialize(TypeRecordKind Kind,
                                                      ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  uint64_t Offset;
  CV_DESERIALIZE(Data, L, CV_NUMERIC_FIELD(Offset));
  return BaseClassRecord(L->Attrs.getAccess(), L->BaseType, Offset);
}

ErrorOr<VirtualBaseClassRecord>
VirtualBaseClassRecord::deserialize(TypeRecordKind Kind,
                                    ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  uint64_t Offset;
  uint64_t Index;
  CV_DESERIALIZE(Data, L, CV_NUMERIC_FIELD(Offset), CV_NUMERIC_FIELD(Index));
  return VirtualBaseClassRecord(L->Attrs.getAccess(), L->BaseType, L->VBPtrType,
                                Offset, Index);
}

ErrorOr<ListContinuationRecord>
ListContinuationRecord::deserialize(TypeRecordKind Kind,
                                    ArrayRef<uint8_t> &Data) {
  const Layout *L = nullptr;
  CV_DESERIALIZE(Data, L);
  return ListContinuationRecord(L->ContinuationIndex);
}

//===----------------------------------------------------------------------===//
// Type index remapping
//===----------------------------------------------------------------------===//

static bool remapIndex(ArrayRef<TypeIndex> IndexMap, TypeIndex &Idx) {
  // Simple types are unchanged.
  if (Idx.isSimple())
    return true;
  unsigned MapPos = Idx.getIndex() - TypeIndex::FirstNonSimpleIndex;
  if (MapPos < IndexMap.size()) {
    Idx = IndexMap[MapPos];
    return true;
  }

  // This type index is invalid. Remap this to "not translated by cvpack",
  // and return failure.
  Idx = TypeIndex(SimpleTypeKind::NotTranslated, SimpleTypeMode::Direct);
  return false;
}

bool ModifierRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, ModifiedType);
}

bool ProcedureRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, ReturnType);
  Success &= remapIndex(IndexMap, ArgumentList);
  return Success;
}

bool MemberFunctionRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, ReturnType);
  Success &= remapIndex(IndexMap, ClassType);
  Success &= remapIndex(IndexMap, ThisType);
  Success &= remapIndex(IndexMap, ArgumentList);
  return Success;
}

bool MemberFuncIdRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, ClassType);
  Success &= remapIndex(IndexMap, FunctionType);
  return Success;
}

bool ArgListRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  for (TypeIndex &Str : StringIndices)
    Success &= remapIndex(IndexMap, Str);
  return Success;
}

bool MemberPointerInfo::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, ContainingType);
}

bool PointerRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, ReferentType);
  if (isPointerToMember())
    Success &= MemberInfo.remapTypeIndices(IndexMap);
  return Success;
}

bool NestedTypeRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, Type);
}

bool ArrayRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, ElementType);
  Success &= remapIndex(IndexMap, IndexType);
  return Success;
}

bool TagRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, FieldList);
}

bool ClassRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= TagRecord::remapTypeIndices(IndexMap);
  Success &= remapIndex(IndexMap, DerivationList);
  Success &= remapIndex(IndexMap, VTableShape);
  return Success;
}

bool EnumRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= TagRecord::remapTypeIndices(IndexMap);
  Success &= remapIndex(IndexMap, UnderlyingType);
  return Success;
}

bool BitFieldRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, Type);
}

bool VFTableShapeRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return true;
}

bool TypeServer2Record::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return true;
}

bool StringIdRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, Id);
}

bool FuncIdRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, ParentScope);
  Success &= remapIndex(IndexMap, FunctionType);
  return Success;
}

bool UdtSourceLineRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, UDT);
  Success &= remapIndex(IndexMap, SourceFile);
  return Success;
}

bool UdtModSourceLineRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, UDT);
  Success &= remapIndex(IndexMap, SourceFile);
  return Success;
}

bool BuildInfoRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  for (TypeIndex &Arg : ArgIndices)
    Success &= remapIndex(IndexMap, Arg);
  return Success;
}

bool VFTableRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, CompleteClass);
  Success &= remapIndex(IndexMap, OverriddenVFTable);
  return Success;
}

bool OneMethodRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, Type);
  return Success;
}

bool MethodOverloadListRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  for (OneMethodRecord &Meth : Methods)
    if ((Success = Meth.remapTypeIndices(IndexMap)))
      return Success;
  return Success;
}

bool OverloadedMethodRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, MethodList);
}

bool DataMemberRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, Type);
}

bool StaticDataMemberRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, Type);
}

bool EnumeratorRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return true;
}

bool VFPtrRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, Type);
}

bool BaseClassRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, Type);
}

bool VirtualBaseClassRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  bool Success = true;
  Success &= remapIndex(IndexMap, BaseType);
  Success &= remapIndex(IndexMap, VBPtrType);
  return Success;
}

bool ListContinuationRecord::remapTypeIndices(ArrayRef<TypeIndex> IndexMap) {
  return remapIndex(IndexMap, ContinuationIndex);
}
