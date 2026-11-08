//---------------------------------------------------------------------------
// Replacement for Borland "try { ... } __finally { ... }".
// bcc64x (Win64x / Clang) cannot mix SEH __try/__finally with C++ try/catch
// in one function, so try/__finally blocks were converted to:
//
//   {
//     FINALLY_GUARD([&]() { <finally code> });
//     <try code>
//   }
//
// The finally code runs when the scope is left (normally, by return or by
// exception). If it throws while another exception is already propagating,
// its exception is swallowed (as the original exception wins anyway).
//---------------------------------------------------------------------------
#ifndef FinallyH
#define FinallyH
#include <exception>
#include <utility>
//---------------------------------------------------------------------------
template<typename TFunc>
class TFinallyGuard
{
public:
  explicit TFinallyGuard(TFunc && Func) :
    FFunc(std::move(Func)), FUncaught(std::uncaught_exceptions())
  {
  }

  ~TFinallyGuard() noexcept(false)
  {
    if (std::uncaught_exceptions() > FUncaught)
    {
      try
      {
        FFunc();
      }
      catch (...)
      {
      }
    }
    else
    {
      FFunc();
    }
  }

  TFinallyGuard(const TFinallyGuard &) = delete;
  TFinallyGuard & operator=(const TFinallyGuard &) = delete;

private:
  TFunc FFunc;
  int FUncaught;
};
//---------------------------------------------------------------------------
#define FINALLY_GUARD_CAT2(A, B) A##B
#define FINALLY_GUARD_CAT(A, B) FINALLY_GUARD_CAT2(A, B)
#define FINALLY_GUARD(FUNC) \
  TFinallyGuard FINALLY_GUARD_CAT(FinallyGuard_, __COUNTER__)(FUNC)
//---------------------------------------------------------------------------
#endif
