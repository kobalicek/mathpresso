// [MathPresso]
// Mathematical Expression Parser and JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MATHPRESSO_BUILD_EXPORT

// [Dependencies]
#include "./mpast_p.h"
#include "./mpeval_p.h"
#include "./mpoptimizer_p.h"

namespace mathpresso {

// MathPresso - AstOptimizer
// =========================

AstOptimizer::AstOptimizer(AstBuilder* ast, ErrorReporter* error_reporter)
  : AstVisitor(ast),
    _error_reporter(error_reporter) {}
AstOptimizer::~AstOptimizer() {}

Error AstOptimizer::on_block(AstBlock* node) {
  // Prevent removing nodes that are not stored in pure `AstBlock`. For example
  // function call inherits from `AstBlock`, but it needs each expression passed.
  bool alterable = node->node_type() == kAstNodeBlock;

  uint32_t i = 0;
  uint32_t cur_count = node->size();
  uint32_t old_count;

  while (i < cur_count) {
    MATHPRESSO_PROPAGATE(on_node(node->child_at(i)));

    old_count = cur_count;
    cur_count = node->size();

    if (cur_count < old_count) {
      if (!alterable)
        return MATHPRESSO_TRACE_ERROR(kErrorInvalidState);
      continue;
    }

    if (alterable && (node->child_at(i)->is_imm())) {
      _ast->delete_node(node->remove_at(i));
      cur_count--;
      continue;
    }

    i++;
  }

  return kErrorOk;
}

Error AstOptimizer::on_var_decl(AstVarDecl* node) {
  AstSymbol* sym = node->symbol();

  if (node->child()) {
    MATHPRESSO_PROPAGATE(on_node(node->child()));
    AstNode* child = node->child();

    if (child->is_imm())
      sym->set_value(static_cast<AstImm*>(child)->value());
  }

  return kErrorOk;
}

Error AstOptimizer::on_var(AstVar* node) {
  AstSymbol* sym = node->symbol();

  if (sym->is_assigned() && !node->has_node_flag(kAstNodeHasSideEffect)) {
    AstImm* imm = _ast->new_node<AstImm>(sym->value());
    _ast->delete_node(node->parent()->replace_node(node, imm));
  }

  return kErrorOk;
}

Error AstOptimizer::on_imm(AstImm* node) {
  (void)node;
  return kErrorOk;
}

Error AstOptimizer::on_unary_op(AstUnaryOp* node) {
  const OpInfo& op = OpInfo::get(node->op_type());

  MATHPRESSO_PROPAGATE(on_node(node->child()));
  AstNode* child = node->child();

  if (child->is_imm()) {
    AstImm* child = static_cast<AstImm*>(node->child());
    double value = child->value();

    switch (node->op_type()) {
      case kOpNeg      : value = -value; break;
      case kOpNot      : value = (value == 0); break;

      case kOpIsNan    : value = mp_is_nan(value); break;
      case kOpIsInf    : value = mp_is_inf(value); break;
      case kOpIsFinite : value = mp_is_finite(value); break;
      case kOpSignBit  : value = mp_sign_bit(value); break;

      case kOpRound    : value = mp_round(value); break;
      case kOpRoundEven: value = mp_round_even(value); break;
      case kOpTrunc    : value = mp_trunc(value); break;
      case kOpFloor    : value = mp_floor(value); break;
      case kOpCeil     : value = mp_ceil(value); break;

      case kOpAbs      : value = mp_abs(value); break;
      case kOpExp      : value = mp_exp(value); break;
      case kOpLog      : value = mp_log(value); break;
      case kOpLog2     : value = mp_log2(value); break;
      case kOpLog10    : value = mp_log10(value); break;
      case kOpSqrt     : value = mp_sqrt(value); break;
      case kOpFrac     : value = mp_frac(value); break;
      case kOpRecip    : value = mp_recip(value); break;

      case kOpSin      : value = mp_sin(value); break;
      case kOpCos      : value = mp_cos(value); break;
      case kOpTan      : value = mp_tan(value); break;

      case kOpSinh     : value = mp_sinh(value); break;
      case kOpCosh     : value = mp_cosh(value); break;
      case kOpTanh     : value = mp_tanh(value); break;

      case kOpAsin     : value = mp_asin(value); break;
      case kOpAcos     : value = mp_acos(value); break;
      case kOpAtan     : value = mp_atan(value); break;

      default:
        return _error_reporter->on_error(kErrorInvalidState, node->position(),
          "Invalid unary operation '%s'.", op.name);
    }

    child->set_value(value);

    node->unlink_child();
    node->parent()->replace_node(node, child);

    _ast->delete_node(node);
  }
  else if (child->node_type() == kAstNodeUnaryOp && node->op_type() == child->op_type()) {
    // Simplify `-(-(x))` -> `x`.
    if (node->op_type() == kOpNeg) {
      AstNode* child_of_child = static_cast<AstUnaryOp*>(child)->unlink_child();
      node->parent()->replace_node(node, child_of_child);
      _ast->delete_node(node);
    }
  }

  return kErrorOk;
}

Error AstOptimizer::on_binary_op(AstBinaryOp* node) {
  const OpInfo& op = OpInfo::get(node->op_type());

  AstNode* left = node->left();
  AstNode* right = node->right();

  if (op.is_assignment())
    left->add_node_flags(kAstNodeHasSideEffect);

  MATHPRESSO_PROPAGATE(on_node(left));
  left = node->left();

  MATHPRESSO_PROPAGATE(on_node(right));
  right = node->right();

  bool l_is_imm = left->is_imm();
  bool r_is_imm = right->is_imm();

  // If both nodes are values it's easy, just fold them into a single one.
  if (l_is_imm && r_is_imm) {
    AstImm* l_node = static_cast<AstImm*>(left);
    AstImm* r_node = static_cast<AstImm*>(right);

    double l_val = l_node->value();
    double r_val = r_node->value();
    double result = 0.0;

    switch (node->op_type()) {
      case kOpEq      : result = l_val == r_val; break;
      case kOpNe      : result = l_val != r_val; break;
      case kOpLt      : result = l_val < r_val; break;
      case kOpLe      : result = l_val <= r_val; break;
      case kOpGt      : result = l_val > r_val; break;
      case kOpGe      : result = l_val >= r_val; break;
      case kOpAdd     : result = l_val + r_val; break;
      case kOpSub     : result = l_val - r_val; break;
      case kOpMul     : result = l_val * r_val; break;
      case kOpDiv     : result = l_val / r_val; break;
      case kOpMod     : result = mp_mod(l_val, r_val); break;
      case kOpAvg     : result = mp_avg(l_val, r_val); break;
      case kOpMin     : result = mp_min(l_val, r_val); break;
      case kOpMax     : result = mp_max(l_val, r_val); break;
      case kOpPow     : result = mp_pow(l_val, r_val); break;
      case kOpAtan2   : result = mp_atan2(l_val, r_val); break;
      case kOpHypot   : result = mp_hypot(l_val, r_val); break;
      case kOpCopySign: result = mp_copy_sign(l_val, r_val); break;

      default:
        return _error_reporter->on_error(kErrorInvalidState, node->position(),
          "Invalid binary operation '%s'.", op.name);
    }

    l_node->set_value(result);
    node->unlink_left();
    node->parent()->replace_node(node, l_node);

    _ast->delete_node(node);
  }
  // There is still a little optimization opportunity.
  else if (l_is_imm) {
    AstImm* l_node = static_cast<AstImm*>(left);
    double val = l_node->value();

    if ((val == 0.0 && (op.flags & kOpFlagNopIfLZero)) ||
        (val == 1.0 && (op.flags & kOpFlagNopIfLOne))) {
      node->unlink_right();
      node->parent()->replace_node(node, right);

      _ast->delete_node(node);
    }
  }
  else if (r_is_imm) {
    AstImm* r_node = static_cast<AstImm*>(right);
    double val = r_node->value();

    // Evaluate an assignment.
    if (op.is_assignment() && left->is_var()) {
      AstSymbol* sym = static_cast<AstVar*>(left)->symbol();
      if (op.type == kOpAssign || sym->is_assigned()) {
        sym->set_value(val);
        sym->mark_assigned();
      }
    }
    else {
      if ((val == 0.0 && (op.flags & kOpFlagNopIfRZero)) ||
          (val == 1.0 && (op.flags & kOpFlagNopIfROne))) {
        node->unlink_left();
        node->parent()->replace_node(node, left);

        _ast->delete_node(node);
      }
    }
  }

  return kErrorOk;
}

Error AstOptimizer::on_invoke(AstCall* node) {
  AstSymbol* sym = node->symbol();
  uint32_t i, count = node->size();

  bool allConst = true;
  for (i = 0; i < count; i++) {
    MATHPRESSO_PROPAGATE(on_node(node->child_at(i)));
    allConst &= node->child_at(i)->is_imm();
  }

  if (allConst && count <= 8) {
    AstImm** args = reinterpret_cast<AstImm**>(node->children());

    void* fn = sym->func_ptr();
    double result = 0.0;

    #define ARG(n) args[n]->value()
    switch (count) {
      case 0: result = ((Arg0Func)fn)(); break;
      case 1: result = ((Arg1Func)fn)(ARG(0)); break;
      case 2: result = ((Arg2Func)fn)(ARG(0), ARG(1)); break;
      case 3: result = ((Arg3Func)fn)(ARG(0), ARG(1), ARG(2)); break;
      case 4: result = ((Arg4Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3)); break;
      case 5: result = ((Arg5Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4)); break;
      case 6: result = ((Arg6Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)); break;
      case 7: result = ((Arg7Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6)); break;
      case 8: result = ((Arg8Func)fn)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7)); break;
    }
    #undef ARG

    AstNode* replacement = _ast->new_node<AstImm>(result);
    node->parent()->replace_node(node, replacement);
    _ast->delete_node(node);
  }

  return kErrorOk;
}

} // {mathpresso}
