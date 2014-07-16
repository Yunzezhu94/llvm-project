//===- lib/FileFormat/MachO/ArchHandler.h ---------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MachONormalizedFile.h"

#include "lld/Core/LLVM.h"
#include "lld/Core/Reference.h"
#include "lld/Core/Simple.h"
#include "lld/ReaderWriter/MachOLinkingContext.h"

#include "llvm/ADT/Triple.h"

#ifndef LLD_READER_WRITER_MACHO_ARCH_HANDLER_H
#define LLD_READER_WRITER_MACHO_ARCH_HANDLER_H

namespace lld {
namespace mach_o {

///
/// The ArchHandler class handles all architecture specific aspects of
/// mach-o linking.
///
class ArchHandler {
public:
  virtual ~ArchHandler();
  
  /// There is no public interface to subclasses of ArchHandler, so this
  /// is the only way to instantiate an ArchHandler.
  static std::unique_ptr<ArchHandler> create(MachOLinkingContext::Arch arch);

  /// Get (arch specific) kind strings used by Registry.
  virtual const Registry::KindStrings *kindStrings() = 0;
  
  /// Convert mach-o Arch to Reference::KindArch. 
  virtual Reference::KindArch kindArch() = 0;

  /// Used by StubPass to update References to shared library functions
  /// to be references to a stub.
  virtual bool isCallSite(const Reference &) = 0;

  /// Used by GOTPass to locate GOT References
  virtual bool isGOTAccess(const Reference &, bool &canBypassGOT) {
    return false;
  }

  /// Used by GOTPass to update GOT References
  virtual void updateReferenceToGOT(const Reference *, bool targetIsNowGOT) {}

  /// Used by normalizedFromAtoms() to know where to generated rebasing and 
  /// binding info in final executables.
  virtual bool isPointer(const Reference &) = 0;
  
  /// Used by normalizedFromAtoms() to know where to generated lazy binding 
  /// info in final executables.
  virtual bool isLazyPointer(const Reference &);

  /// Returns true if the specified relocation is paired to the next relocation.
  virtual bool isPairedReloc(const normalized::Relocation &) = 0;

  /// Prototype for a helper function.  Given a sectionIndex and address, 
  /// finds the atom and offset with that atom of that address. 
  typedef std::function<std::error_code (uint32_t sectionIndex, uint64_t addr, 
                        const lld::Atom **, Reference::Addend *)> 
                        FindAtomBySectionAndAddress;
  
  /// Prototype for a helper function.  Given a symbolIndex, finds the atom
  /// representing that symbol.
  typedef std::function<std::error_code (uint32_t symbolIndex, 
                        const lld::Atom **)> FindAtomBySymbolIndex;

  /// Analyzes a relocation from a .o file and returns the info
  /// (kind, target, addend) needed to instantiate a Reference.
  /// Two helper functions are passed as parameters to find the target atom
  /// given a symbol index or address.
  virtual std::error_code 
          getReferenceInfo(const normalized::Relocation &reloc,
                           const DefinedAtom *inAtom,
                           uint32_t offsetInAtom,
                           uint64_t fixupAddress, bool swap,
                           FindAtomBySectionAndAddress atomFromAddress,
                           FindAtomBySymbolIndex atomFromSymbolIndex,
                           Reference::KindValue *kind, 
                           const lld::Atom **target, 
                           Reference::Addend *addend) = 0;

  /// Analyzes a pair of relocations from a .o file and returns the info
  /// (kind, target, addend) needed to instantiate a Reference.
  /// Two helper functions are passed as parameters to find the target atom
  /// given a symbol index or address.
  virtual std::error_code 
      getPairReferenceInfo(const normalized::Relocation &reloc1,
                           const normalized::Relocation &reloc2,
                           const DefinedAtom *inAtom,
                           uint32_t offsetInAtom,
                           uint64_t fixupAddress, bool swap,
                           FindAtomBySectionAndAddress atomFromAddress,
                           FindAtomBySymbolIndex atomFromSymbolIndex,
                           Reference::KindValue *kind, 
                           const lld::Atom **target, 
                           Reference::Addend *addend) = 0;

  /// Fixup an atom when generating a final linked binary.
  virtual void applyFixup(Reference::KindNamespace ns, Reference::KindArch arch,
                          Reference::KindValue kindValue, uint64_t addend,
                          uint8_t *location, uint64_t fixupAddress,
                          uint64_t targetAddress, uint64_t inAtomAddress) = 0;

  struct ReferenceInfo {
    Reference::KindArch arch;
    uint16_t            kind;
    uint32_t            offset;
    int32_t             addend;
  };

  /// Table of architecture specific information for creating stubs.
  struct StubInfo {
    const char*     binderSymbolName;
    ReferenceInfo   lazyPointerReferenceToHelper;
    ReferenceInfo   lazyPointerReferenceToFinal;
    ReferenceInfo   nonLazyPointerReferenceToBinder;
    uint8_t         codeAlignment;
    
    uint32_t        stubSize;
    uint8_t         stubBytes[16];
    ReferenceInfo   stubReferenceToLP;
    
    uint32_t        stubHelperSize;
    uint8_t         stubHelperBytes[16];
    ReferenceInfo   stubHelperReferenceToImm;
    ReferenceInfo   stubHelperReferenceToHelperCommon;
    
    uint32_t        stubHelperCommonSize;
    uint8_t         stubHelperCommonBytes[36];
    ReferenceInfo   stubHelperCommonReferenceToCache;
    ReferenceInfo   stubHelperCommonReferenceToBinder;
  };

  virtual const StubInfo &stubInfo() = 0;

protected:
  ArchHandler();

  static std::unique_ptr<mach_o::ArchHandler> create_x86_64();
  static std::unique_ptr<mach_o::ArchHandler> create_x86();
  static std::unique_ptr<mach_o::ArchHandler> create_arm();

  // Handy way to pack mach-o r_type and other bit fields into one 16-bit value.
  typedef uint16_t RelocPattern;
  enum {
    rScattered = 0x8000,
    rPcRel     = 0x4000,
    rExtern    = 0x2000,
    rLength1   = 0x0000,
    rLength2   = 0x0100,
    rLength4   = 0x0200,
    rLength8   = 0x0300
  };
  static RelocPattern relocPattern(const normalized::Relocation &reloc);

  static int16_t  readS16(bool swap, const uint8_t *addr);
  static int32_t  readS32(bool swap, const uint8_t *addr);
  static uint32_t readU32(bool swap, const uint8_t *addr);
  static int64_t  readS64(bool swap, const uint8_t *addr);
};

} // namespace mach_o
} // namespace lld

#endif // LLD_READER_WRITER_MACHO_ARCH_HANDLER_H
