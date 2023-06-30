LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := armpatch
LOCAL_SRC_FILES := obj/local/$(TARGET_ARCH_ABI)/libarmpatch.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libz
LOCAL_SRC_FILES := zlib/adler32.c zlib/crc32.c zlib/gzclose.c zlib/gzlib.c zlib/gzread.c zlib/gzwrite.c zlib/infback.c zlib/inffast.c zlib/inflate.c zlib/inftrees.c zlib/trees.c zlib/uncompr.c zlib/zutil.c
LOCAL_C_INCLUDES += $(LOCAL_PATH)/zlib
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= wolfssl
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(LOCAL_PATH)/wolfssl
LOCAL_CFLAGS:= -DHAVE_FFDHE_2048 -DWOLFSSL_TLS13 -DHAVE_TLS_EXTENSIONS -DHAVE_SUPPORTED_CURVES -DTFM_TIMING_RESISTANT -DECC_TIMING_RESISTANT -DWC_RSA_BLINDING -DHAVE_AESGCM -DWOLFSSL_SHA512 -DWOLFSSL_SHA384 -DHAVE_HKDF -DNO_DSA -DHAVE_ECC -DTFM_ECC256 -DECC_SHAMIR -DWC_RSA_PSS -DWOLFSSL_BASE64_ENCODE -DNO_RC4 -DWOLFSSL_SHA224 -DWOLFSSL_SHA3 -DHAVE_POLY1305 -DHAVE_ONE_TIME_AUTH -DHAVE_CHACHA -DHAVE_HASHDRBG -DHAVE_TLS_EXTENSIONS -DHAVE_SUPPORTED_CURVES -DHAVE_EXTENDED_MASTER -DHAVE_SNI -DHAVE_ALPN -DWOLFSSL_JNI -DWOLFSSL_DTLS -DOPENSSL_EXTRA -DOPENSSL_ALL -DHAVE_EX_DATA -DHAVE_CRL -DHAVE_OCSP -DHAVE_CRL_MONITOR -DPERSIST_SESSION_CACHE -DPERSIST_CERT_CACHE -DATOMIC_USER -DHAVE_PK_CALLBACKS -DWOLFSSL_CERT_EXT -DWOLFSSL_CERT_GEN -DHAVE_ENCRYPT_THEN_MAC -DNO_MD4 -DWOLFSSL_ENCRYPTED_KEYS -DUSE_FAST_MATH -DNO_DES3 -DKEEP_PEER_CERT -DSESSION_CERTS -DHAVE_SESSION_TICKET -DSIZEOF_LONG=4 -DSIZEOF_LONG_LONG=8 -Os -fomit-frame-pointer
LOCAL_C_INCLUDES += \
    external/wolfssl/wolfssl \
    external/wolfssl \
LOCAL_SRC_FILES:= \
    wolfssl/src/crl.c \
    wolfssl/src/internal.c \
    wolfssl/src/keys.c \
    wolfssl/src/ocsp.c \
    wolfssl/src/sniffer.c \
    wolfssl/src/ssl.c \
    wolfssl/src/tls.c \
    wolfssl/src/tls13.c \
    wolfssl/src/wolfio.c \
    wolfssl/src/dtls.c
LOCAL_SRC_FILES+= \
    wolfssl/wolfcrypt/src/aes.c \
    wolfssl/wolfcrypt/src/arc4.c \
    wolfssl/wolfcrypt/src/asm.c \
    wolfssl/wolfcrypt/src/asn.c \
    wolfssl/wolfcrypt/src/blake2b.c \
    wolfssl/wolfcrypt/src/blake2s.c \
    wolfssl/wolfcrypt/src/camellia.c \
    wolfssl/wolfcrypt/src/chacha.c \
    wolfssl/wolfcrypt/src/chacha20_poly1305.c \
    wolfssl/wolfcrypt/src/cmac.c \
    wolfssl/wolfcrypt/src/coding.c \
    wolfssl/wolfcrypt/src/compress.c \
    wolfssl/wolfcrypt/src/cpuid.c \
    wolfssl/wolfcrypt/src/cryptocb.c \
    wolfssl/wolfcrypt/src/curve25519.c \
    wolfssl/wolfcrypt/src/des3.c \
    wolfssl/wolfcrypt/src/dh.c \
    wolfssl/wolfcrypt/src/dsa.c \
    wolfssl/wolfcrypt/src/ecc.c \
    wolfssl/wolfcrypt/src/ecc_fp.c \
    wolfssl/wolfcrypt/src/ed25519.c \
    wolfssl/wolfcrypt/src/error.c \
    wolfssl/wolfcrypt/src/fe_low_mem.c \
    wolfssl/wolfcrypt/src/fe_operations.c \
    wolfssl/wolfcrypt/src/ge_low_mem.c \
    wolfssl/wolfcrypt/src/ge_operations.c \
    wolfssl/wolfcrypt/src/hash.c \
    wolfssl/wolfcrypt/src/hmac.c \
    wolfssl/wolfcrypt/src/integer.c \
    wolfssl/wolfcrypt/src/kdf.c \
    wolfssl/wolfcrypt/src/logging.c \
    wolfssl/wolfcrypt/src/md2.c \
    wolfssl/wolfcrypt/src/md4.c \
    wolfssl/wolfcrypt/src/md5.c \
    wolfssl/wolfcrypt/src/memory.c \
    wolfssl/wolfcrypt/src/pkcs12.c \
    wolfssl/wolfcrypt/src/pkcs7.c \
    wolfssl/wolfcrypt/src/poly1305.c \
    wolfssl/wolfcrypt/src/pwdbased.c \
    wolfssl/wolfcrypt/src/random.c \
    wolfssl/wolfcrypt/src/ripemd.c \
    wolfssl/wolfcrypt/src/rsa.c \
    wolfssl/wolfcrypt/src/sha.c \
    wolfssl/wolfcrypt/src/sha256.c \
    wolfssl/wolfcrypt/src/sha3.c \
    wolfssl/wolfcrypt/src/sha512.c \
    wolfssl/wolfcrypt/src/signature.c \
    wolfssl/wolfcrypt/src/sp_arm32.c \
    wolfssl/wolfcrypt/src/sp_arm64.c \
    wolfssl/wolfcrypt/src/sp_armthumb.c \
    wolfssl/wolfcrypt/src/sp_c32.c \
    wolfssl/wolfcrypt/src/sp_c64.c \
    wolfssl/wolfcrypt/src/sp_cortexm.c \
    wolfssl/wolfcrypt/src/sp_int.c \
    wolfssl/wolfcrypt/src/sp_x86_64.c \
    wolfssl/wolfcrypt/src/srp.c \
    wolfssl/wolfcrypt/src/tfm.c \
    wolfssl/wolfcrypt/src/wc_encrypt.c \
    wolfssl/wolfcrypt/src/wc_pkcs11.c \
    wolfssl/wolfcrypt/src/wc_port.c \
    wolfssl/wolfcrypt/src/wolfevent.c \
    wolfssl/wolfcrypt/src/wolfmath.c
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := curl
LOCAL_SHARED_LIBRARIES := wolfssl libz
LOCAL_CFLAGS += -DCURL_USE_WOLFSSL -DHAVE_INTTYPES_H -DHAVE_SYS_IOCTL_H -DHAVE_SYS_STAT_H -DHAVE_SYS_TIME_H -DHAVE_SYS_TYPES_H -DHAVE_CONFIG_H
LOCAL_C_INCLUDES += $(LOCAL_PATH)/curl $(LOCAL_PATH)/curl/include $(LOCAL_PATH)/curl/lib
LOCAL_SRC_FILES += \
    curl/lib/strtoofft.c \
    curl/lib/timediff.c \
    curl/lib/nonblock.c \
    curl/lib/warnless.c \
    curl/lib/curl_multibyte.c \
    curl/lib/version_win32.c \
    curl/lib/dynbuf.c \
    curl/src/slist_wc.c \
    curl/src/tool_binmode.c \
    curl/src/tool_bname.c \
    curl/src/tool_cb_dbg.c \
    curl/src/tool_cb_hdr.c \
    curl/src/tool_cb_prg.c \
    curl/src/tool_cb_rea.c \
    curl/src/tool_cb_see.c \
    curl/src/tool_cb_wrt.c \
    curl/src/tool_cfgable.c \
    curl/src/tool_dirhie.c \
    curl/src/tool_doswin.c \
    curl/src/tool_easysrc.c \
    curl/src/tool_filetime.c \
    curl/src/tool_findfile.c \
    curl/src/tool_formparse.c \
    curl/src/tool_getparam.c \
    curl/src/tool_getpass.c \
    curl/src/tool_help.c \
    curl/src/tool_helpers.c \
    curl/src/tool_libinfo.c \
    curl/src/tool_listhelp.c \
    curl/src/tool_main.c \
    curl/src/tool_msgs.c \
    curl/src/tool_operate.c \
    curl/src/tool_operhlp.c \
    curl/src/tool_paramhlp.c \
    curl/src/tool_parsecfg.c \
    curl/src/tool_progress.c \
    curl/src/tool_stderr.c \
    curl/src/tool_strdup.c \
    curl/src/tool_setopt.c \
    curl/src/tool_sleep.c \
    curl/src/tool_urlglob.c \
    curl/src/tool_util.c \
    curl/src/tool_vms.c \
    curl/src/tool_writeout.c \
    curl/src/tool_writeout_json.c \
    curl/src/tool_xattr.c
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION     := .cpp .cc
LOCAL_SHARED_LIBRARIES  := armpatch curl
LOCAL_MODULE            := AML
LOCAL_SRC_FILES         := main.cpp interface.cpp aml.cpp modpaks.cpp \
                           modslist.cpp icfg.cpp vtable_hooker.cpp \
                           mod/logger.cpp mod/config.cpp

 ## FLAGS ##
LOCAL_CFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG -D__AML -std=c17 -mthumb
LOCAL_CXXFLAGS += -O2 -mfloat-abi=softfp -DNDEBUG -D__AML -std=c++17 -mthumb -fexceptions
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include $(LOCAL_PATH)/curl $(LOCAL_PATH)/curl/include $(LOCAL_PATH)/wolfssl
LOCAL_LDLIBS += -llog -ldl -landroid

# Uncomment these two lines to add IL2CPP support! (NOT WORKING)
#    LOCAL_SRC_FILES += il2cpp/gc.cpp il2cpp/functions.cpp
#    LOCAL_CFLAGS += -D__IL2CPPUTILS
# Uncomment these two lines to add IL2CPP support! (NOT WORKING)

 ## BUILD ##
include $(BUILD_SHARED_LIBRARY)
