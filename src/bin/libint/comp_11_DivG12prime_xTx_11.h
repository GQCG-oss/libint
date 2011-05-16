
#ifndef _libint2_src_bin_libint_cr11divg12primextx11_h_
#define _libint2_src_bin_libint_cr11divg12primextx11_h_

#include <generic_rr.h>
#include <integral_11_11.h>
#include <gaussoper.h>

#define USE_R12kR12lG12 1

using namespace std;

namespace libint2 {

  /** Compute relation for 2-e integrals of the DivG12prime_xTx operators.
  */
  template <class BFSet>
    class CR_11_DivG12prime_xTx_11 : public GenericRecurrenceRelation< CR_11_DivG12prime_xTx_11<BFSet>,
                                                             BFSet,
                                                             GenIntegralSet_11_11<BFSet,DivG12prime_xTx,mType> >
    {
    public:
      typedef CR_11_DivG12prime_xTx_11 ThisType;
      typedef BFSet BasisFunctionType;
      typedef GenIntegralSet_11_11<BFSet,DivG12prime_xTx,mType> TargetType;
      typedef GenericRecurrenceRelation<ThisType,BFSet,TargetType> ParentType;
      friend class GenericRecurrenceRelation<ThisType,BFSet,TargetType>;
      static const unsigned int max_nchildren = 36;

      using ParentType::Instance;
    private:
      using RecurrenceRelation::expr_;
      using RecurrenceRelation::nflops_;
      using ParentType::target_;
      using ParentType::is_simple;

      /// Constructor is private, used by ParentType::Instance that mainains registry of these objects
      CR_11_DivG12prime_xTx_11(const SafePtr<TargetType>&, unsigned int dir);
      /// This relation is not directional
      static bool directional() { return false; }
      static std::string descr() { return "CR"; }

    };

  template <class F>
    CR_11_DivG12prime_xTx_11<F>::CR_11_DivG12prime_xTx_11(const SafePtr<TargetType>& Tint,
                                      unsigned int dir) :
    ParentType(Tint,dir)
    {
      using namespace libint2::algebra;
      using namespace libint2::prefactor;
      using namespace libint2::braket;

      // kinetic energy of which electron?
      const int i = target_->oper()->descr().I();
      F a(Tint->bra(0,0));
      F b(Tint->ket(0,0));
      F c(Tint->bra(1,0));
      F d(Tint->ket(1,0));

      if (a.contracted() ||
          b.contracted() ||
          c.contracted() ||
          d.contracted() ||
          target_->oper()->descr().contracted())
        return;

#if USE_R12kR12lG12
      if (i == 0)
      {
        typedef GenIntegralSet_11_11<BasisFunctionType,R12kR12lG12,EmptySet> ChildType;
        ChildFactory<ThisType,ChildType> factory(this);
        for(int bxyz=0; bxyz<3; ++bxyz) {
          for(int kxyz=0; kxyz<3; ++kxyz) {
            R12k_R12l_G12_Descr descr(unit_intvec3(bxyz),unit_intvec3(kxyz));
            factory.wedge( Nabla1(_pbra(a,c),bxyz) , Nabla1(_pket(b,d),kxyz), EmptySet(), R12kR12lG12(descr));
          }
        }
      }
      if (i == 1)
      {
        typedef GenIntegralSet_11_11<BasisFunctionType,R12kR12lG12,EmptySet> ChildType;
        ChildFactory<ThisType,ChildType> factory(this);
        for(int bxyz=0; bxyz<3; ++bxyz) {
          for(int kxyz=0; kxyz<3; ++kxyz) {
            R12k_R12l_G12_Descr descr(unit_intvec3(bxyz),unit_intvec3(kxyz));
            factory.wedge( Nabla2(_pbra(a,c),bxyz) , Nabla2(_pket(b,d),kxyz), EmptySet(), R12kR12lG12(descr));
          }
        }
      }
#else
      // |Nabla1.G12' ac ) ^ |Nabla1.G12' bd )
      if (i == 0)
      {
        typedef GenIntegralSet_11_11<BasisFunctionType,R12kG12,mType> ChildType;
        ChildFactory<ThisType,ChildType> factory(this);
        factory.wedge( R12vec_dot_Nabla1(_pbra(a,c)) , R12vec_dot_Nabla1(_pket(b,d)), mType(0u), R12kG12(0));
      }
      // |Nabla2.G12' ac ) ^ |Nabla2.G12' bd )
      if (i == 1)
      {
        typedef GenIntegralSet_11_11<BasisFunctionType,R12kG12,mType> ChildType;
        ChildFactory<ThisType,ChildType> factory(this);
        factory.wedge( R12vec_dot_Nabla2(_pbra(a,c)) , R12vec_dot_Nabla2(_pket(b,d)), mType(0u), R12kG12(0));
      }
#endif
      if (is_simple()) expr_ *= Scalar(-4.0) * Scalar("gamma_bra") * Scalar("gamma_ket");

    }

};


#endif
