dnl
dnl ac_crypto.m4
dnl
dnl
dnl Set up crypto library options 
dnl
dnl


dnl LAC_CRYPTO_ARGS()

AC_DEFUN(LAC_CRYPTO_ARGS,
[
])

dnl LAC_CRYPTO()

AC_DEFUN(LAC_CRYPTO,
[
    AC_REQUIRE([AC_CANONICAL_HOST])
    AC_REQUIRE([LAC_CPU])
    AC_REQUIRE([AC_PROG_CC])
    LAC_CRYPTO_ARGS
    LAC_CRYPTO_SET

    LAC_SUBSTITUTE_VAR(DATE)
    LAC_DEFINE_VAR(OPENSSLDIR)
    LAC_DEFINE_VAR(SIXTY_FOUR_BIT_LONG)
    LAC_DEFINE_VAR(SIXTY_FOUR_BIT)
    LAC_DEFINE_VAR(THIRTY_TWO_BIT)
    LAC_DEFINE_VAR(DES_PTR)
    LAC_DEFINE_VAR(DES_RISC1)
    LAC_DEFINE_VAR(DES_RISC2)
    LAC_DEFINE_VAR(DES_UNROLL)
    LAC_DEFINE_VAR(DES_LONG)
    LAC_DEFINE_VAR(BN_LLONG)
    LAC_DEFINE_VAR(BF_PTR)
    LAC_DEFINE_VAR(RC4_CHUNK)
    LAC_DEFINE_VAR(RC4_INDEX)
    LAC_DEFINE_VAR(RC4_INT)
    LAC_DEFINE_VAR(RC2_INT)
    LAC_DEFINE_VAR(MD2_INT)
    LAC_DEFINE_VAR(IDEA_INT)
])


dnl LAC_CRYPTO_SET
AC_DEFUN(LAC_CRYPTO_SET,
[
    # defaults:

    lac_OPENSSLDIR="\"$GLOBUS_LOCATION\""
    lac_DATE="`date`"
    lac_SIXTY_FOUR_BIT_LONG=""
    lac_SIXTY_FOUR_BIT=""
    lac_THIRTY_TWO_BIT="1"
    lac_DES_PTR=""
    lac_DES_RISC1=""
    lac_DES_RISC2=""
    lac_DES_UNROLL=""
    lac_DES_LONG="unsigned long"
    lac_BN_LLONG=""
    lac_BF_PTR=""
    lac_RC4_CHUNK=""
    lac_RC4_INDEX=""
    lac_RC4_INT="unsigned int"
    lac_RC2_INT="unsigned int"
    lac_MD2_INT="unsigned int"
    lac_IDEA_INT="unsigned int"

    case ${host} in
        *solaris*)
            case ${lac_cv_CPU} in
                *sun4m*|*sun4d*)
                    if test "$GCC" = "1"; then
                        lac_BN_LLONG="1"
                        lac_RC4_INT="unsigned char"
                        lac_RC4_CHUNK="unsigned long"
                        lac_DES_UNROLL="1" 
                        lac_BF_PTR="1"
                    else
                        lac_BN_LLONG="1"
                        lac_RC4_INT="unsigned char"
                        lac_RC4_CHUNK="unsigned long"
                        lac_DES_PTR="1"
                        lac_DES_RISC1="1"
                        lac_DES_UNROLL="1" 
                        lac_BF_PTR="1"
                    fi
                ;;
                *sun4u*)
                    if test "$GCC" = "1"; then
                        lac_BN_LLONG="1"
                        lac_RC4_INT="unsigned char"
                        lac_RC4_CHUNK="unsigned long"
                        lac_DES_UNROLL="1" 
                        lac_BF_PTR="1"
                    else
                        lac_BN_LLONG="1"
                        lac_RC4_INT="unsigned char"
                        lac_RC4_CHUNK="unsigned long long"
                        lac_DES_PTR="1"
                        lac_DES_RISC1="1"
                        lac_DES_UNROLL="1" 
                        lac_BF_PTR="1"
                    fi
                ;;
                *x86*)
                    if test "$GCC" = "1"; then
                        lac_BN_LLONG="1"
                        lac_RC4_INDEX="1"
                        lac_DES_PTR="1"
                        lac_DES_RISC1="1"
                        lac_DES_UNROLL="1" 
                    else
                        lac_BN_LLONG="1"
                        lac_RC4_INT="unsigned char"
                        lac_RC4_CHUNK="unsigned long"
                        lac_DES_PTR="1"
                        lac_DES_UNROLL="1" 
                        lac_BF_PTR="1"
                    fi
                ;;
            esac
        ;;   
        *linux*)
            case ${lac_cv_CPU} in
                *sun4m*|*sun4d*)
                    # gcc
                    lac_BN_LLONG="1"
                    lac_RC4_INT="unsigned char"
                    lac_RC4_CHUNK="unsigned long"
                    lac_DES_UNROLL="1" 
                    lac_BF_PTR="1"
                ;;
                *sun4u*)
                    # gcc
                    lac_BN_LLONG="1"
                    lac_RC4_INT="unsigned char"
                    lac_RC4_CHUNK="unsigned long"
                    lac_DES_UNROLL="1" 
                    lac_BF_PTR="1"
                ;;
                *x86*)
                    # gcc
                    lac_BN_LLONG="1"
                    lac_DES_PTR="1"
                    lac_DES_RISC1="1"
                    lac_DES_UNROLL="1"
                    lac_RC4_INDEX="1"
                ;;
                *ia64*)
                    # gcc
                    lac_SIXTY_FOUR_BIT_LONG="1"
                    lac_THIRTY_TWO_BIT=""
                    lac_RC4_INT="unsigned char"
                    lac_RC4_CHUNK="unsigned long"
                ;;
                *alpha*)
                    if test "$GCC" = "1"; then
                        lac_SIXTY_FOUR_BIT_LONG="1"
                        lac_THIRTY_TWO_BIT=""
                        lac_RC4_CHUNK="unsigned long"
                        lac_DES_RISC1="1" 
                        lac_DES_UNROLL="1" 
                    else
                        lac_SIXTY_FOUR_BIT_LONG="1"
                        lac_THIRTY_TWO_BIT=""
                        lac_RC4_INT="unsigned char"
                        lac_RC4_CHUNK="unsigned long"
                        lac_DES_RISC1="1" 
                        lac_DES_UNROLL="1" 
                    fi
                ;;
            esac
        ;;
        *irix64*)
            # gcc and vendor
            lac_RC4_INT="unsigned char"
            lac_RC4_CHUNK="unsigned long"
            lac_DES_RISC2="1"
            lac_DES_PTR="1"
            lac_DES_UNROLL="1"
            lac_BF_PTR="1"
            lac_SIXTY_FOUR_BIT_LONG="1"
            lac_THIRTY_TWO_BIT=""
        ;;
        *irix6*)
            case ${lac_cv_CPU} in
                *mips3* | *mips4* )
                    if test "$GCC" = "1"; then
                        lac_MD2_INT="unsigned char"
                        lac_RC4_INDEX="1"
                        lac_RC4_INT="unsigned char"
                        lac_RC4_CHUNK="unsigned long long"
                        lac_DES_RISC2="1"
                        lac_DES_PTR="1"
                        lac_DES_UNROLL="1"
                        lac_BF_PTR="1"
                        lac_SIXTY_FOUR_BIT="1"
                        lac_THIRTY_TWO_BIT=""
                    else
                        lac_RC4_INT="unsigned char"
                        lac_RC4_CHUNK="unsigned long long"
                        lac_DES_RISC2="1"
                        lac_DES_PTR="1"
                        lac_DES_UNROLL="1"
                        lac_BF_PTR="1"
                        lac_SIXTY_FOUR_BIT="1"
                        lac_THIRTY_TWO_BIT=""
                    fi
                ;;
            esac
        ;;
        *hpux*)
            if test "$GCC" = "1"; then
                lac_BN_LLONG="1"
                lac_DES_PTR="1"
                lac_DES_UNROLL="1"
                lac_DES_RISC1="1"
            else
                lac_MD2_INT="unsigned char"
                lac_RC4_INDEX="1"
                lac_RC4_INT="unsigned char"
                lac_DES_RISC1="1"
                lac_DES_LONG="unsigned int"
                lac_DES_UNROLL="1"
            fi
        ;;
        *-ibm-aix*)
            # gcc and vendor
            lac_BN_LLONG="1"
            lac_RC4_INT="unsigned char"
        ;;
        *-dec-osf*)
            if test "$GCC" = "1"; then
                lac_SIXTY_FOUR_BIT_LONG="1"
                lac_THIRTY_TWO_BIT=""
                lac_RC4_CHUNK="unsigned long"
                lac_DES_UNROLL="1"
                lac_DES_RISC1="1"
            else
                lac_SIXTY_FOUR_BIT_LONG="1"
                lac_THIRTY_TWO_BIT=""
                lac_RC4_CHUNK="unsigned long"
            fi
        ;;
    esac
])







