// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Class to draw a binary FST by producing a text file in dot format, a helper
// class to fstdraw.cc.

#ifndef FST_SCRIPT_DRAW_IMPL_H_
#define FST_SCRIPT_DRAW_IMPL_H_

#include <ostream>
#include <sstream>
#include <string>

#include <fst/fst.h>
#include <fst/util.h>
#include <fst/script/fst-class.h>

namespace fst {

// Print a binary Fst in the dot textual format, helper class for fstdraw.cc
// WARNING: Stand-alone use not recommend.
template <class A>
class FstDrawer {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;

  FstDrawer(const Fst<A> &fst, const SymbolTable *isyms,
            const SymbolTable *osyms, const SymbolTable *ssyms, bool accep,
            string title, float width, float height, bool portrait,
            bool vertical, float ranksep, float nodesep, int fontsize,
            int precision, bool show_weight_one)
      : fst_(fst),
        isyms_(isyms),
        osyms_(osyms),
        ssyms_(ssyms),
        accep_(accep && fst.Properties(kAcceptor, true)),
        ostrm_(nullptr),
        title_(title),
        width_(width),
        height_(height),
        portrait_(portrait),
        vertical_(vertical),
        ranksep_(ranksep),
        nodesep_(nodesep),
        fontsize_(fontsize),
        precision_(precision),
        show_weight_one_(show_weight_one) {}

  // Draw Fst to an output buffer (or stdout if buf = 0)
  void Draw(std::ostream *strm, const string &dest) {
    ostrm_ = strm;
    ostrm_->precision(precision_);
    dest_ = dest;
    StateId start = fst_.Start();
    if (start == kNoStateId) return;

    PrintString("digraph FST {\n");
    if (vertical_)
      PrintString("rankdir = BT;\n");
    else
      PrintString("rankdir = LR;\n");
    PrintString("size = \"");
    Print(width_);
    PrintString(",");
    Print(height_);
    PrintString("\";\n");
    if (!dest_.empty()) PrintString("label = \"" + title_ + "\";\n");
    PrintString("center = 1;\n");
    if (portrait_)
      PrintString("orientation = Portrait;\n");
    else
      PrintString("orientation = Landscape;\n");
    PrintString("ranksep = \"");
    Print(ranksep_);
    PrintString("\";\n");
    PrintString("nodesep = \"");
    Print(nodesep_);
    PrintString("\";\n");
    // initial state first
    DrawState(start);
    for (StateIterator<Fst<A>> siter(fst_); !siter.Done(); siter.Next()) {
      StateId s = siter.Value();
      if (s != start) DrawState(s);
    }
    PrintString("}\n");
  }

 private:
  // Maximum line length in text file.
  static const int kLineLen = 8096;

  void PrintString(const string &s) const { *ostrm_ << s; }

  // Escapes backslash and double quote if these occur in the string. Dot will
  // not deal gracefully with these if they are not escaped.
  inline void EscapeChars(const string &s, string *ns) const {
    const char *c = s.c_str();
    while (*c) {
      if (*c == '\\' || *c == '"') ns->push_back('\\');
      ns->push_back(*c);
      ++c;
    }
  }

  void PrintId(StateId id, const SymbolTable *syms, const char *name) const {
    if (syms) {
      string symbol = syms->Find(id);
      if (symbol == "") {
        FSTERROR() << "FstDrawer: Integer " << id
                   << " is not mapped to any textual symbol"
                   << ", symbol table = " << syms->Name()
                   << ", destination = " << dest_;
        symbol = "?";
      }
      string nsymbol;
      EscapeChars(symbol, &nsymbol);
      PrintString(nsymbol);
    } else {
      string idstr;
      Int64ToStr(id, &idstr);
      PrintString(idstr);
    }
  }

  void PrintStateId(StateId s) const { PrintId(s, ssyms_, "state ID"); }

  void PrintILabel(Label l) const { PrintId(l, isyms_, "arc input label"); }

  void PrintOLabel(Label l) const { PrintId(l, osyms_, "arc output label"); }

  template <class T>
  void Print(T t) const { *ostrm_ << t; }

  void DrawState(StateId s) const {
    Print(s);
    PrintString(" [label = \"");
    PrintStateId(s);
    Weight final = fst_.Final(s);
    if (final != Weight::Zero()) {
      if (show_weight_one_ || (final != Weight::One())) {
        PrintString("/");
        Print(final);
      }
      PrintString("\", shape = doublecircle,");
    } else {
      PrintString("\", shape = circle,");
    }
    if (s == fst_.Start())
      PrintString(" style = bold,");
    else
      PrintString(" style = solid,");
    PrintString(" fontsize = ");
    Print(fontsize_);
    PrintString("]\n");
    for (ArcIterator<Fst<A>> aiter(fst_, s); !aiter.Done(); aiter.Next()) {
      Arc arc = aiter.Value();
      PrintString("\t");
      Print(s);
      PrintString(" -> ");
      Print(arc.nextstate);
      PrintString(" [label = \"");
      PrintILabel(arc.ilabel);
      if (!accep_) {
        PrintString(":");
        PrintOLabel(arc.olabel);
      }
      if (show_weight_one_ || (arc.weight != Weight::One())) {
        PrintString("/");
        Print(arc.weight);
      }
      PrintString("\", fontsize = ");
      Print(fontsize_);
      PrintString("];\n");
    }
  }

  const Fst<A> &fst_;
  const SymbolTable *isyms_;  // ilabel symbol table
  const SymbolTable *osyms_;  // olabel symbol table
  const SymbolTable *ssyms_;  // slabel symbol table
  bool accep_;                // print as acceptor when possible
  std::ostream *ostrm_;       // drawn FST destination
  string dest_;               // drawn FST destination name

  string title_;
  float width_;
  float height_;
  bool portrait_;
  bool vertical_;
  float ranksep_;
  float nodesep_;
  int fontsize_;
  int precision_;
  bool show_weight_one_;

  DISALLOW_COPY_AND_ASSIGN(FstDrawer);
};

}  // namespace fst

#endif  // FST_SCRIPT_DRAW_IMPL_H_
