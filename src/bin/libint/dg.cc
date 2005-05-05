
#include <dg.h>
#include <fstream>
#include <rr.h>
#include <strategy.h>
#include <prefactors.h>
#include <codeblock.h>
#include <default_params.h>

using namespace std;
using namespace libint2;

DirectedGraph::DirectedGraph() :
  stack_(default_size_,SafePtr<DGVertex>()), rrstack_(new RRStack),
  first_free_(0), first_to_compute_()
{
}

DirectedGraph::~DirectedGraph()
{
  reset();
}

void
DirectedGraph::append_target(const SafePtr<DGVertex>& target)
{
  target->make_a_target();
  append_vertex(target);
}

void
DirectedGraph::append_vertex(const SafePtr<DGVertex>& vertex)
{
  try {
    add_vertex(vertex);
  }
  catch (VertexAlreadyOnStack& e) {}
}

void
DirectedGraph::add_vertex(const SafePtr<DGVertex>& vertex) throw(VertexAlreadyOnStack)
{
  vertex_is_on(vertex);

  if (first_free_ == stack_.size()) {
    stack_.resize( stack_.size() + default_size_ );
    cout << "Increased size of DirectedGraph's stack to "
         << stack_.size() << endl;
  }
  char label[80];  sprintf(label,"vertex%d",first_free_);
  vertex->set_graph_label(label);
  stack_[first_free_++] = vertex;
  return;
}

void
DirectedGraph::vertex_is_on(const SafePtr<DGVertex>& vertex) const throw(VertexAlreadyOnStack)
{
  for(int i=0; i<first_free_; i++)
    if(vertex->equiv(stack_[i]))
      throw VertexAlreadyOnStack(stack_[i]);
}

void
DirectedGraph::del_vertex(const SafePtr<DGVertex>& v) throw(CannotPerformOperation)
{
  vector< SafePtr<DGVertex> >::iterator pos = find(stack_.begin(),stack_.end(),v);
  if (pos == stack_.end())
    throw CannotPerformOperation("DirectedGraph::del_vertex() cannot delete vertex");

  if (v->num_exit_arcs() == 0 && v->num_entry_arcs() == 0)
    stack_.erase(pos);
  else
    throw CannotPerformOperation("DirectedGraph::del_vertex() cannot delete vertex");
  --first_free_;
}

void
DirectedGraph::prepare_to_traverse()
{
  for(int i=0; i<first_free_; i++)
    stack_[i]->prepare_to_traverse();
}

void
DirectedGraph::traverse()
{
  // Initialization
  prepare_to_traverse();

  // Start at the targets which don't have parents
  for(int i=0; i<first_free_; i++) {
    if (stack_[i]->is_a_target() && stack_[i]->num_entry_arcs() == 0) {
      SafePtr<DGVertex> vertex_ptr = stack_[i];
      // First, since this target doesn't have parents we can schedule its computation
      schedule_computation(vertex_ptr);
      int nchildren = vertex_ptr->num_exit_arcs();
      for(int c=0; c<nchildren; c++)
        traverse_from(vertex_ptr->exit_arc(c));
    }
  }
}

void
DirectedGraph::traverse_from(const SafePtr<DGArc>& arc)
{
  SafePtr<DGVertex> orig = arc->orig();
  SafePtr<DGVertex> dest = arc->dest();
  if (dest->precomputed())
    return;
  const unsigned int num_tags = dest->tag();
  const unsigned int num_parents = dest->num_entry_arcs();
  /*if (num_tags == 1) {
    int nchildren = dest->num_exit_arcs();
    for(int c=0; c<nchildren; c++)
      traverse_from(dest->exit_arc(c));
      }*/
  if (num_tags == num_parents) {
    schedule_computation(dest);
    int nchildren = dest->num_exit_arcs();
    for(int c=0; c<nchildren; c++)
      traverse_from(dest->exit_arc(c));
  }
}

void
DirectedGraph::schedule_computation(const SafePtr<DGVertex>& vertex)
{
  vertex->set_precalc(SafePtr<DGVertex>());
  vertex->set_postcalc(first_to_compute_);
  if (first_to_compute_ != 0)
    first_to_compute_->set_precalc(vertex);
  first_to_compute_ = vertex;
}


void
DirectedGraph::debug_print_traversal(std::ostream& os) const
{
  SafePtr<DGVertex> current_vertex = first_to_compute_;

  os << "Debug print of traversal order" << endl;

  do {
    os << current_vertex->label() << endl;
    current_vertex = current_vertex->postcalc();
  } while (current_vertex != 0);
}

void
DirectedGraph::print_to_dot(bool symbols, std::ostream& os) const
{
  os << "digraph G {" << endl
     << "  size = \"8,8\"" << endl;
  for(int i=0; i<first_free_; i++) {
    SafePtr<DGVertex> vertex = stack_[i];
    os << "  " << vertex->graph_label()
       << " [ label = \"";
    if (symbols && vertex->symbol_set())
      os << vertex->symbol();
    else
      os << vertex->label();
    os << "\"]" << endl;
  }
  for(int i=0; i<first_free_; i++) {
    SafePtr<DGVertex> vertex = stack_[i];
    unsigned int narcs = vertex->num_exit_arcs();
    for(int a=0; a<narcs; a++) {
      SafePtr<DGVertex> dest = vertex->exit_arc(a)->dest();
      os << "  " << vertex->graph_label() << " -> "
         << dest->graph_label() << endl;
    }
  }

  // Print traversal order using dotted lines
  SafePtr<DGVertex> current_vertex = first_to_compute_;
  if (current_vertex != 0) {
    do {
      SafePtr<DGVertex> next = current_vertex->postcalc();
      if (current_vertex && next) {
        os << "  " << current_vertex->graph_label() << " -> "
           << next->graph_label() << " [ style = dotted ]";
      }
      current_vertex = next;
    } while (current_vertex != 0);
  }

  os << "}" << endl;
}

void
DirectedGraph::reset()
{
  // Reset each vertex, releasing all arcs
  for(int i=0; i<first_free_; i++)
    stack_[i]->reset();
  for(int i=0; i<first_free_; i++)
    stack_[i].reset();
  // if everything went OK then resize stack_ to 0
  stack_.resize(default_size_);
  first_free_ = 0;
  first_to_compute_.reset();
}


/// Apply a strategy to all vertices not yet computed (i.e. which do not have exit arcs)
void
DirectedGraph::apply(const SafePtr<Strategy>& strategy,
                     const SafePtr<Tactic>& tactic)
{
  const int num_vertices_on_graph = first_free_;
  for(int v=0; v<num_vertices_on_graph; v++) {
    if (stack_[v]->num_exit_arcs() != 0 || stack_[v]->precomputed() || !stack_[v]->need_to_compute())
      continue;

    SafePtr<DirectedGraph> this_ptr = SafePtr_from_this();
    SafePtr<RecurrenceRelation> rr0 = strategy->optimal_rr(this_ptr,stack_[v],tactic);
    if (rr0 == 0)
      continue;

    // add children to the graph
    SafePtr<DGVertex> target = rr0->rr_target();
    const int num_children = rr0->num_children();
    for(int c=0; c<num_children; c++) {
      SafePtr<DGVertex> child = rr0->rr_child(c);
      bool new_vertex = true;
      try { add_vertex(child); }
      catch (VertexAlreadyOnStack& e) { child = e.vertex(); new_vertex = false; }
      SafePtr<DGArc> arc(new DGArcRel<RecurrenceRelation>(target,child,rr0));
      target->add_exit_arc(arc);
      if (new_vertex)
        apply_to(child,strategy,tactic);
    }
  }
}

/// Add vertex to graph and apply a strategy to vertex recursively
void
DirectedGraph::apply_to(const SafePtr<DGVertex>& vertex,
                        const SafePtr<Strategy>& strategy,
                        const SafePtr<Tactic>& tactic)
{
  if (vertex->precomputed() || !vertex->need_to_compute())
    return;
  SafePtr<RecurrenceRelation> rr0 = strategy->optimal_rr(SafePtr_from_this(),vertex,tactic);
  if (rr0 == 0)
    return;

  SafePtr<DGVertex> target = rr0->rr_target();
  const int num_children = rr0->num_children();
  for(int c=0; c<num_children; c++) {
    SafePtr<DGVertex> child = rr0->rr_child(c);
    bool new_vertex = true;
    try { add_vertex(child); }
    catch (VertexAlreadyOnStack& e) { child = e.vertex(); new_vertex = false; }
    SafePtr<DGArc> arc(new DGArcRel<RecurrenceRelation>(target,child,rr0));
    target->add_exit_arc(arc);
    if (new_vertex)
      apply_to(child,strategy,tactic);
  }
}

// Optimize out simple recurrence relations
void
DirectedGraph::optimize_rr_out()
{
  replace_rr_with_expr();
  remove_trivial_arithmetics();
  handle_trivial_nodes();
  remove_disconnected_vertices();
}

// Replace recurrence relations with expressions
void
DirectedGraph::replace_rr_with_expr()
{
  const unsigned int nvertices = first_free_;
  for(int v=0; v<nvertices; v++) {

    SafePtr<DGVertex> vertex = stack_[v];
    if (vertex->num_exit_arcs()) {
      SafePtr<DGArc> arc0 = vertex->exit_arc(0);
      SafePtr<DGArcRR> arc0_cast = dynamic_pointer_cast<DGArcRR,DGArc>(arc0);
      if (arc0_cast == 0)
        continue;
      SafePtr<RecurrenceRelation> rr = arc0_cast->rr();

      // Optimize if the recurrence relation is simple and the target and
      // children are of the same type
      if (rr->is_simple() && rr->invariant_type()) {

        unsigned int nchildren = rr->num_children();
        unsigned int nexpr = rr->num_expr();

        // Remove arcs connecting this vertex to children
        vertex->del_exit_arcs();

        // and instead insert expressions
        for(int e=0; e<nexpr; e++) {
          SafePtr< AlgebraicOperator<DGVertex> > rr_expr_cast = dynamic_pointer_cast<AlgebraicOperator<DGVertex>,DGVertex>(rr->rr_expr(e));
          if (rr_expr_cast)
            insert_expr_at(vertex,rr_expr_cast);
          else
            throw runtime_error("DirectedGraph::optimize_rr_out() -- expression of invalid type");
        }

      }
    }
  }
}

void
DirectedGraph::insert_expr_at(const SafePtr<DGVertex>& where, const SafePtr< AlgebraicOperator<DGVertex> >& expr)
{
  typedef AlgebraicOperator<DGVertex> ExprType;
  
  SafePtr<DGVertex> expr_vertex = dynamic_pointer_cast<DGVertex,ExprType>(expr);
  bool new_vertex = true;
  try { add_vertex(expr_vertex); }
  catch (VertexAlreadyOnStack& e) { expr_vertex = e.vertex(); new_vertex = false; }
  SafePtr<DGArc> arc(new DGArcDirect(where,expr_vertex));
  where->add_exit_arc(arc);
  if (!new_vertex)
    return;

  // See if left operand is also an operator
  SafePtr<ExprType> left_cast = dynamic_pointer_cast<ExprType,DGVertex>(expr->left());
  if (left_cast)
    insert_expr_at(expr_vertex,left_cast);
  else {
    SafePtr<DGVertex> left_vertex = expr->left();
    bool new_vertex = true;
    try { add_vertex(left_vertex); }
    catch (VertexAlreadyOnStack& e) { left_vertex = e.vertex(); new_vertex = false; }
    SafePtr<DGArc> arc(new DGArcDirect(expr_vertex,left_vertex));
    expr_vertex->add_exit_arc(arc);
  }

  // See if right operand is also an operator
  SafePtr<ExprType> right_cast = dynamic_pointer_cast<ExprType,DGVertex>(expr->right());
  if (right_cast)
    insert_expr_at(expr_vertex,right_cast);
  else {
    SafePtr<DGVertex> right_vertex = expr->right();
    bool new_vertex = true;
    try { add_vertex(right_vertex); }
    catch (VertexAlreadyOnStack& e) { right_vertex = e.vertex(); new_vertex = false; }
    SafePtr<DGArc> arc(new DGArcDirect(expr_vertex,right_vertex));
    expr_vertex->add_exit_arc(arc);
  }

}

// Replace recurrence relations with expressions
void
DirectedGraph::remove_trivial_arithmetics()
{
  const unsigned int nvertices = first_free_;
  for(int v=0; v<nvertices; v++) {

    SafePtr<DGVertex> vertex = stack_[v];
    SafePtr< AlgebraicOperator<DGVertex> > oper_cast = dynamic_pointer_cast<AlgebraicOperator<DGVertex>,DGVertex>(vertex);
    if (oper_cast) {

      SafePtr<DGVertex> left = oper_cast->exit_arc(0)->dest();
      SafePtr<DGVertex> right = oper_cast->exit_arc(1)->dest();

      // 1.0 * x = x
      if (left->equiv(prefactors.N_i[1])) {
        remove_vertex_at(vertex,right);
      }

      // x * 1.0 = x
      if (right->equiv(prefactors.N_i[1])) {
        remove_vertex_at(vertex,left);
      }

      // NOTE : more cases to come
    }
  }
}

//
// Handles "trivial" nodes. A node is trivial is it satisfies the following conditions:
// 1) it has only one child
// 2) the exit arc is of a trivial type (DGArvDirect or IntegralSet_to_Integral applied to node of size 1)
//
// By "handling" I mean either removing the node from the graph or making a node refer to another node so that
// no code is generated for it.
//
void
DirectedGraph::handle_trivial_nodes()
{
  const unsigned int nvertices = first_free_;
  for(int v=0; v<nvertices; v++) {

    SafePtr<DGVertex> vertex = stack_[v];
    if (vertex->num_exit_arcs() != 1)
      continue;
    SafePtr<DGArc> arc = vertex->exit_arc(0);

    // Is the exit arc DGArcDirect?
    {
      SafePtr<DGArcDirect> arc_cast = dynamic_pointer_cast<DGArcDirect,DGArc>(arc);
      if (arc_cast) {
        // remove the vertex, if possible
        try { remove_vertex_at(vertex,arc->dest()); }
        catch (CannotPerformOperation& c) {
        }
      }
    }

    // Is the exit arc DGArcRel<IntegralSet_to_Integrals> and vertex->size() == 1?
    {
      if (vertex->size() == 1) {
        SafePtr<DGArcRR> arc_cast = dynamic_pointer_cast<DGArcRR,DGArc>(arc);
        if (arc_cast) {
          SafePtr<RecurrenceRelation> rr = arc_cast->rr();
          SafePtr<IntegralSet_to_Integrals_base> rr_cast = dynamic_pointer_cast<IntegralSet_to_Integrals_base,RecurrenceRelation>(rr);
          if (rr_cast)
            vertex->refer_this_to(arc->dest());
        }
      }
    }
    
      // NOTE : more cases to come
  }
}


// If v1 and v2 are connected by DGArcDirect and all entry arcs to v1 are of the DGArcDirect type as well,
// this function will reattach all arcs extering v1 to v2 and remove v1 from the graph alltogether.
void
DirectedGraph::remove_vertex_at(const SafePtr<DGVertex>& v1, const SafePtr<DGVertex>& v2) throw(CannotPerformOperation)
{
  typedef vector<SafePtr<DGArc> > arcvec;
  typedef arcvec::iterator aiter;
  // Collect all entry arcs in a container
  arcvec v1_entry;
  // Verify that all entry arcs are DGArcDirect
  for(int i=0; i<v1->num_entry_arcs(); i++) {
    // See if this is a direct arc -- otherwise cannot do this
    SafePtr<DGArc> arc = v1->entry_arc(i);
    SafePtr<DGArcDirect> arc_cast = dynamic_pointer_cast<DGArcDirect,DGArc>(arc);
    if (arc_cast == 0)
      throw CannotPerformOperation("DirectedGraph::remove_vertex_at() -- cannot remove vertex");
    v1_entry.push_back(v1->entry_arc(i));
  }

  // Verify that v1 and v2 are connected by an arc and it is the only arc exiting v1
  /*if (v1->num_exit_arcs() != 1 || v1->exit_arc(0)->dest() != v2)
    throw CannotPerformOperation("DirectedGraph::remove_vertex_at() -- cannot remove vertex");*/

  // See if this is a direct arc -- otherwise cannot do this
  SafePtr<DGArc> arc = v1->exit_arc(0);
  SafePtr<DGArcDirect> arc_cast = dynamic_pointer_cast<DGArcDirect,DGArc>(arc);
  if (arc_cast == 0)
    throw CannotPerformOperation("DirectedGraph::remove_vertex_at() -- cannot remove vertex");

  //
  // OK, now do work!
  //

  // Reconnect each of v1's entry arcs to v2
  for(arcvec::iterator i=v1_entry.begin(); i != v1_entry.end(); i++) {
    SafePtr<DGVertex> parent = (*i)->orig();
    SafePtr<DGArcDirect> new_arc(new DGArcDirect(parent,v2));
    parent->replace_exit_arc(*i,new_arc);
  }

  // and fully disconnect this vertex
  v1->detach();
}

void
DirectedGraph::remove_disconnected_vertices()
{
  for(int i=0; i<first_free_; i++) {
    SafePtr<DGVertex> vertex = stack_[i];
    if (vertex->num_entry_arcs() == 0 && vertex->num_exit_arcs() == 0) {
      try { del_vertex(vertex); }
      catch (CannotPerformOperation) {
        continue;
      }
      // first_free_ was decremented, so need to decrease i as well
      --i;
    }
  }
}


//
//
//
void
DirectedGraph::generate_code(const SafePtr<CodeContext>& context, const SafePtr<MemoryManager>& memman,
                             const SafePtr<ImplicitDimensions>& dims, const SafePtr<CodeSymbols>& args,
                             const std::string& label,
                             std::ostream& decl, std::ostream& def)
{
  decl << context->std_header();
  std::string comment("This code computes "); comment += label; comment += "\n";
  if (context->comments_on())
    decl << context->comment(comment) << endl;

  std::string function_name = label_to_funcname(label);
  function_name = context->label_to_name(function_name);

  decl << context->code_prefix();
  std::string func_decl;
  ostringstream oss;
  oss << context->type_name<void>() << " "
      << function_name << "(Libint_t* libint";
  unsigned int nargs = args->n();
  for(unsigned int a=0; a<nargs; a++) {
    oss << ", " << context->type_name<double*>() << " "
        << args->symbol(a);
  }
  if (!dims->high_is_static()) {
    oss << ", " << context->type_name<int>() << " "
        << dims->high()->id();
  }
  if (!dims->low_is_static()) {
    oss << ", " << context->type_name<int>() << " "
        <<dims->low()->id();
  }
  oss << ")";
  func_decl = oss.str();
  
  decl << func_decl << context->end_of_stat() << endl;
  decl << context->code_postfix();

  //
  // Generate function's definition
  //

  // include standard headers
  def << context->std_header();
  // include declarations for all fucntion calls:
  // 1) extract all unresolved RecurrenceRelation's
  // 2) include their headers into the current definition file
  // 3) add them to rrstack_ so that later their code can be
  //    generated
  SafePtr<RRStack> rrstack(new RRStack);
  extract_rr(rrstack);
  for(RRStack::citer_type rrkey=rrstack->begin(); rrkey!=rrstack->end(); rrkey++) {
    string function_name = (*rrkey).second->label();
    def << "#include <"
        << context->label_to_name(function_name)
        << ".h>" << endl;
  }
  def << endl;
  rrstack_->add(rrstack);
  
  def << context->code_prefix();
  def << func_decl << context->open_block() << endl;
  def << context->std_function_header();

  context->reset();
  allocate_mem(memman,dims,1);
  assign_symbols(context,dims);
  print_def(context,def,dims);
  def << context->close_block() << endl;
  def << context->code_postfix();
}

void
DirectedGraph::allocate_mem(const SafePtr<MemoryManager>& memman,
                            const SafePtr<ImplicitDimensions>& dims,
                            unsigned int min_size_to_alloc)
{
  // First, reset tag counters
  for(int i=0; i<first_free_; i++)
    stack_[i]->prepare_to_traverse();

  // Second, allocate space for all targets
#if 0
  for(int i=0; i<first_free_; i++) {
    SafePtr<DGVertex> vertex = stack_[i];
    if (vertex->is_a_target())
      vertex->set_address(memman->alloc(vertex->size()));
  }
#endif

  //
  // Go through the traversal order and at each step tag every child
  // Once a child receives same number of tags as the number of parents,
  // it can be deallocated
  //
  SafePtr<DGVertex> vertex = first_to_compute_;
  do {
    if (!vertex->symbol_set() && !vertex->address_set() && !vertex->precomputed() &&
        vertex->need_to_compute() && vertex->size() > min_size_to_alloc) {
      vertex->set_address(memman->alloc(vertex->size()));
      const unsigned int nchildren = vertex->num_exit_arcs();
      for(int c=0; c<nchildren; c++) {
        SafePtr<DGVertex> child = vertex->exit_arc(c)->dest();
        const unsigned int ntags = child->tag();
        if (ntags == child->num_entry_arcs() && child->address_set()) {
          memman->free(child->address());
        }
      }
    }
    vertex = vertex->postcalc();
  } while (vertex != 0);

}

namespace {
  std::string stack_symbol(const SafePtr<CodeContext>& ctext, const DGVertex::Address& address, unsigned int size,
                           const std::string& low_rank, const std::string& veclen, bool yesvec,
                           const std::string& prefix = "libint->stack")
  {
    ostringstream oss;
    oss << prefix << "[((hsi*" << size << "+"
        << ctext->stack_address(address) << ")*" << low_rank << "+lsi)*"
        << veclen << (yesvec ? "+vi" : "") << "]";
    return oss.str();
  }
};

void
DirectedGraph::assign_symbols(const SafePtr<CodeContext>& context, const SafePtr<ImplicitDimensions>& dims)
{
  std::ostringstream os;
  const std::string null_str("");
  
  // Generate the label for the rank of the low dimension
  std::string low_rank;
  std::string veclen;
  if (dims->low_is_static()) {
    typedef CTimeEntity<int> ctype;
    SafePtr<ctype> cptr = dynamic_pointer_cast<ctype,Entity>(dims->low());
    ostringstream oss;
    oss << cptr->value();
    low_rank = oss.str();
  }
  else {
    low_rank = dims->low()->id();
  }
  if (dims->vecdim_is_static()) {
    typedef CTimeEntity<int> ctype;
    SafePtr<ctype> cptr = dynamic_pointer_cast<ctype,Entity>(dims->vecdim());
    ostringstream oss;
    oss << cptr->value();
    veclen = oss.str();
  }
  else {
    veclen = dims->vecdim()->id();
  }

  // First, set symbols for all vertices which have address assigned
  for(int i=0; i<first_free_; i++) {
    SafePtr<DGVertex> vertex = stack_[i];
    if (!vertex->symbol_set() && vertex->address_set()) {
      //os.str(null_str);
      //os << "libint->stack[(hsi*" << vertex->size() << "+"
      //   << context->stack_address(vertex->address()) << ")*" << low_rank << "+lsi]";
      //vertex->set_symbol(os.str());
      vertex->set_symbol(stack_symbol(context,vertex->address(),vertex->size(),low_rank,veclen,false));
    }
  }

  // Second, find all nodes which were unrolled using IntegralSet_to_Integrals:
  // 1) such nodes do not need symbols generated since they never appear in the code expicitly
  // 2) children of such nodes have symbols that depend on the parent's address
  for(int i=0; i<first_free_; i++) {
    SafePtr<DGVertex> vertex = stack_[i];
    if (vertex->num_exit_arcs() == 0)
      continue;
    SafePtr<DGArc> arc = vertex->exit_arc(0);
    SafePtr<DGArcRR> arc_rr = dynamic_pointer_cast<DGArcRR,DGArc>(arc);
    if (arc_rr == 0)
      continue;
    SafePtr<RecurrenceRelation> rr = arc_rr->rr();
    SafePtr<IntegralSet_to_Integrals_base> iset_to_i = dynamic_pointer_cast<IntegralSet_to_Integrals_base,RecurrenceRelation>(rr);
    if (iset_to_i == 0) {
      continue;
    }
    else {
      unsigned int nchildren = vertex->num_exit_arcs();
      for(int c=0; c<nchildren; c++) {
        SafePtr<DGVertex> child = vertex->exit_arc(c)->dest();
        // If a child is precomputed -- its symbol will be set as usual
        if (!child->precomputed()) {
          if (vertex->address_set()) {
            //os.str(null_str);
            //os << "libint->stack[" << context->stack_address(vertex->address()+c) << "]";
            //child->set_symbol(os.str());
            child->set_symbol(stack_symbol(context,vertex->address()+c,vertex->size(),low_rank,veclen,true));
          }
          else {
            //os.str(null_str);
            //os << vertex->symbol() << "[" << context->stack_address(c) << "]";
            //child->set_symbol(os.str());
            child->set_symbol(stack_symbol(context,c,vertex->size(),low_rank,veclen,true,vertex->symbol()));
          }
        }
      }
      vertex->refer_this_to(vertex->exit_arc(0)->dest());
      vertex->reset_symbol();
    }
  }

  // then process all other symbols
  for(int i=0; i<first_free_; i++) {
    SafePtr<DGVertex> vertex = stack_[i];
    if (vertex->symbol_set()) {
      continue;
    }

    // test if the vertex is an operator
    {
      typedef AlgebraicOperator<DGVertex> oper;
      SafePtr<oper> ptr_cast = dynamic_pointer_cast<oper,DGVertex>(vertex);
      if (ptr_cast) {
        vertex->set_symbol(context->unique_name<EntityTypes::FP>());
        continue;
      }
    }

    // test if the vertex is a static quantity, like a constant
    {
      typedef CTimeEntity<double> cdouble;
      SafePtr<cdouble> ptr_cast = dynamic_pointer_cast<cdouble,DGVertex>(vertex);
      if (ptr_cast) {
        vertex->set_symbol(ptr_cast->label());
        continue;
      }
    }

    // test if the vertex is precomputed runtime quantity, like a geometric parameter
    if (vertex->precomputed()) {
      std::string symbol("libint->");
      symbol += context->label_to_name(vertex->label());
      symbol += "[vi]";
      vertex->set_symbol(symbol);
      continue;
    }

    // test if the vertex is other runtime quantity
    {
      typedef RTimeEntity<double> cdouble;
      SafePtr<cdouble> ptr_cast = dynamic_pointer_cast<cdouble,DGVertex>(vertex);
      if (ptr_cast) {
        vertex->set_symbol(ptr_cast->label());
        continue;
      }
    }

  }
}

void
DirectedGraph::print_def(const SafePtr<CodeContext>& context, std::ostream& os,
                         const SafePtr<ImplicitDimensions>& dims)
{
  std::ostringstream oss;
  const std::string null_str("");

  std::string varname("hsi");
  SafePtr<ForLoop> hsi_loop(new ForLoop(context,varname,dims->high(),SafePtr<Entity>(new CTimeEntity<int>("0",0))));
  os << hsi_loop->open();
  
  varname = "lsi";
  SafePtr<ForLoop> lsi_loop(new ForLoop(context,varname,dims->low(),SafePtr<Entity>(new CTimeEntity<int>("0",0))));
  os << lsi_loop->open();
  
  varname = "vi";
  SafePtr<ForLoop> vi_loop(new ForLoop(context,varname,dims->vecdim(),SafePtr<Entity>(new CTimeEntity<int>("0",0))));
  os << vi_loop->open();

  unsigned int nflops = 0;
  SafePtr<DGVertex> current_vertex = first_to_compute_;
  do {

    // for every vertex that has a defined symbol, hence must be defined in code
    if (current_vertex->symbol_set()) {

      // print algebraic expression
      {
        typedef AlgebraicOperator<DGVertex> oper_type;
        SafePtr<oper_type> oper_ptr = dynamic_pointer_cast<oper_type,DGVertex>(current_vertex);
        if (oper_ptr) {

          if (context->comments_on()) {
            oss.str(null_str);
            oss << current_vertex->label() << " = "
                << oper_ptr->exit_arc(0)->dest()->label()
                << oper_ptr->label()
                << oper_ptr->exit_arc(1)->dest()->label();
            os << context->comment(oss.str()) << endl;
          }

          // Type declaration
          os << context->declare(context->type_name<double>(),
                                 current_vertex->symbol());
          // expression
          SafePtr<DGVertex> left_arg = oper_ptr->exit_arc(0)->dest();
          SafePtr<DGVertex> right_arg = oper_ptr->exit_arc(1)->dest();
          os << context->assign_binary_expr(current_vertex->symbol(),left_arg->symbol(),oper_ptr->label(),right_arg->symbol());

          nflops++;

          goto next;
        }
      }
      
      // print simple assignment statement
      if (current_vertex->num_exit_arcs() == 1) {
        typedef DGArcDirect arc_type;
        SafePtr<arc_type> arc_ptr = dynamic_pointer_cast<arc_type,DGArc>(current_vertex->exit_arc(0));
        if (arc_ptr) {

          if (context->comments_on()) {
            oss.str(null_str);
            oss << current_vertex->label() << " = "
                << arc_ptr->dest()->label();
            os << context->comment(oss.str()) << endl;
          }

          os << context->assign(current_vertex->symbol(),
                                arc_ptr->dest()->symbol());

          goto next;
        }
      }

      // print out a recurrence relation
      {
        typedef DGArcRR arc_type;
        SafePtr<arc_type> arc_ptr = dynamic_pointer_cast<arc_type,DGArc>(current_vertex->exit_arc(0));
        if (arc_ptr) {
          
          SafePtr<RecurrenceRelation> rr = arc_ptr->rr();
          os << rr->spfunction_call(context,dims);
          /*os << context->label_to_name(rr->label()) << "(libint, "
          << context->value_to_pointer(current_vertex->symbol());
          const unsigned int nchildren = rr->num_children();
          for(int c=0; c<nchildren; c++) {
            os << ", " << context->value_to_pointer(rr->rr_child(c)->symbol());
          }
          os << ")" << context->end_of_stat() << endl;*/
          
          goto next;
        }
      }

      {
        throw std::runtime_error("DirectedGraph::print_def() -- cannot handle this vertex yet");
      }
    }

    next:
    current_vertex = current_vertex->postcalc();
  } while (current_vertex != 0);

  // Print out the number of flops
  oss.str(null_str);
  oss << "Number of flops = " << nflops;
  os << context->comment(oss.str()) << endl;

  // Now pass back all targets through the Libint_t object
  unsigned int ntargets = 0;
  for(int i=0; i<first_free_; i++) {
    SafePtr<DGVertex> vertex = stack_[i];
    if (vertex->is_a_target()) {
      os << "libint->targets[" << ntargets << "] = "
      << context->value_to_pointer(vertex->symbol()) << context->end_of_stat() << endl;
      ++ntargets;
    }
  }
  
  os << vi_loop->close();
  os << lsi_loop->close();
  os << hsi_loop->close();

}


void
DirectedGraph::extract_rr(SafePtr<RRStack>& rrstack) const
{
  // Loop over all vertices
  for(int i=0; i<first_free_; i++) {
    ver_ptr v = stack_[i];
    // for every vertex with children
    if (v->num_exit_arcs() > 0) {
      // if it must be computed using a RR
      SafePtr<DGArc> arc = v->exit_arc(0);
      //SafePtr<DGArcRR> arcrr = dynamic_pointer_cast<DGArc,DGArcRR>(arc);
      using namespace boost;
      using namespace boost::detail;
      SafePtr<DGArcRR> arcrr = boost::shared_ptr<DGArcRR>(arc,boost::detail::dynamic_cast_tag());
      if (arcrr != 0) {
        SafePtr<RecurrenceRelation> rr = arcrr->rr();
        // and the RR is complex (i.e. likely to result in a function call)
        if (!rr->is_simple())
          // add it to the RRStack
          rrstack->find(rr);
      }
    }
  }
}

void
DirectedGraph::generate_rr_code(const SafePtr<CodeContext>& context,
                                const std::string& prefix)
{
  for(RRStack::citer_type it = rrstack_->begin(); it!=rrstack_->end(); it++) {
    SafePtr<RecurrenceRelation> rr = (*it).second;
    std::string rrlabel = rr->label();
    cout << " generating code for " << context->label_to_name(rrlabel) << endl;
    
    std::string decl_filename(prefix + context->label_to_name(rrlabel));  decl_filename += ".h";
    std::string def_filename(prefix + context->label_to_name(rrlabel));  def_filename += ".cc";
    std::basic_ofstream<char> declfile(decl_filename.c_str());
    std::basic_ofstream<char> deffile(def_filename.c_str());
    
    rr->generate_code(context,ImplicitDimensions::default_dims(),declfile,deffile);
    
    declfile.close();
    deffile.close();
  }
}

