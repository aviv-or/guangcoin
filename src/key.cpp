//Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "arith_uint256.h"
#include "crypto/common.h"
#include "crypto/hmac_sha512.h"
#include "pubkey.h"
#include "random.h"

#include "picnic/picnic.h"
//TODOGUANG - picnic keys

//#include <secp256k1.h>
//#include <secp256k1_recovery.h>

//static secp256k1_context* secp256k1_context_sign = NULL;


bool CKey::Check(const unsigned char *vch) {
	//not needed as we make keys via picnic api
    //return secp256k1_ec_seckey_verify(secp256k1_context_sign, vch);
	return true;
}

//TODOGUANG - for PICNIC, making a new key makes a new keypair by default
//  - pubkey generation is not deterministic by default (it consists partly of a random text)
//   - bitcoin assumes pubkeys are deterministic, so we pick a high-entropy text deterministically for a given key
void CKey::MakeNewKey(bool fCompressedIn) {

    //based on unpicking of 
    //picnic_keygen(LowMC_256_256_10_38_UR, picnic_publickey_t* pk,
                  //picnic_privatekey_t* sk)

    //we just do the secure generation of the 96 bytes of random data needed for privkey + plaintext 
 //TOOO REPLACE THIS GUANG
    //do {
    GetStrongRandBytes(keydata.data(), keydata.size());
    //} while (!Check(keydata.data()));
    fValid = true;
    fCompressed = fCompressedIn;
}

bool CKey::SetPrivKey(const CPrivKey &privkey, bool fCompressedIn) {
    //f (!ec_privkey_import_der(secp256k1_context_sign, (unsigned char*)begin(), &privkey[0], privkey.size()))
    //    return false;
    //fCompressed = fCompressedIn;
    //fValid = true;
    if (!fCompressedIn) return false;
    memcpy(&keystore[0], &privkey[0], 96);
    return true;
}

// generated by PICNIC internally when MakeNewKey
CPrivKey CKey::GetPrivKey() const {
    assert(fValid);
    CPrivKey privkey;
    privkey.resize(96);
    memcpy(&privkey[0], &keystore[0], 96);
    return privkey;
}

CPubKey CKey::GetPubKey() const {
    assert(fValid);
    //picnic_publickey_t pubkey;
    lowmcparams_t lowmcparams;
    int ret = picnicParamsToLowMCParams(LowMC_256_256_10_38_UR, &lowmcparams);
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to initialize LowMC parameters\n");
        fflush(stderr);
        return ret;
    }
    CPubKey result;
    result.resize(64);  
    //TODOGUANG - we make the pubkey on generation [although, we can always choose to make more pubkeys for a given PICNIC secret - up to 2^32 of them!
    //first half of public key is the last 32 bytes of our keystore
    memcpy(&result[0], &keystore[64], 32) ;
    //second half is its encryption by the first 64 bytes (the private key proper)
    LowMCEnc((uint32_t*) &keystore[64], (uint32_t*) result[32], (uint32_t*) &keystore[0], &lowmcparams);
    //picnic_write_public_key(const picnic_publickey_t* key, uint8_t* buf, size_t buflen);

    return result;
}

bool CKey::Sign(const uint256 &hash, std::vector<unsigned char>& vchSig, uint32_t test_case) const {

	// sign digest - not as secure as a full signature from PICNIC  TODOGUANG 



    if (!fValid)
        return false;
    vchSig.resize(195458);
    size_t nSigLen = 195458;
    
    //picnic_sign(picnic_privatekey_t* sk, const uint8_t* message, size_t message_len,
    //            uint8_t* signature, size_t* signature_len);
    // sk is our local secret key, hopeflly

	//FROM PICNICSIGN
	ENSURE_LIBRARY_INITIALIZED();

    int ret;
    transform_t transform = get_transform(LowMC_256_256_10_38_UR);
    signature_t* sig = (signature_t*)malloc(sizeof(signature_t));
    lowmcparams_t lowmcparams;

    ret = picnicParamsToLowMCParams(LowMC_256_256_10_38_UR, &lowmcparams);
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to initialize LowMC parameters\n");
        fflush(stderr);
        freeSignature(sig);
        free(sig);
        return ret;
    }

    allocateSignature(sig, &lowmcparams); //what does this do? --- we would like sig to be just &vchSig[0]in the sign function call below, but can't be if this modifies it again! 
    if (sig == NULL) {
        return -1;
    }
    uint8_t ciphertext[32];
    LowMCEnc((uint32_t*) &keystore[64], (uint32_t*) ciphertext, (uint32_t*) &keystore[0], &lowmcparams)
    ret = sign((uint32_t*)&keystore[0], (uint32_t*) ciphertext, (uint32_t*)&keystore[64], hash.begin(), 32, sig, transform, &lowmcparams);
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to create signature\n");
        fflush(stderr);
        freeSignature(sig);
        free(sig);
        return -1;
    }

    ret = serializeSignature(sig, &vchSig[0], 195458, transform, &lowmcparams);
    if (ret == -1) {
        fprintf(stderr, "Failed to serialize signature\n");
        fflush(stderr);
        freeSignature(sig);
        free(sig);
        return -1;
    }
    nSigLen = rett;
    freeSignature(sig);
    free(sig);

	//PICNICSIGN
    vchSig.resize(nSigLen);
 
    return true;
}

bool CKey::VerifyPubKey(const CPubKey& pubkey) const {
    if (pubkey.IsCompressed() != fCompressed) {
        return false;
    }
    unsigned char rnd[8];
    std::string str = "Guangcoin key verification\n";
    GetRandBytes(rnd, sizeof(rnd));
    uint256 hash;
    CHash256().Write((unsigned char*)str.data(), str.size()).Write(rnd, sizeof(rnd)).Finalize(hash.begin());
    std::vector<unsigned char> vchSig;
    Sign(hash, vchSig);
    return pubkey.Verify(hash, vchSig);
}

bool CKey::SignCompact(const uint256 &hash, std::vector<unsigned char>& vchSig) const {
    if (!fValid)
        return false;
    return false; //can't do this with a PICNIC signature
}

bool CKey::Load(CPrivKey &privkey, CPubKey &vchPubKey, bool fSkipCheck=false) {
    //copy the bits to the right places, check that 
    memcmp(&privkey[64],&vchPubKey[0],32) //should be identical

    if (!ec_privkey_import_der(secp256k1_context_sign, (unsigned char*)begin(), &privkey[0], privkey.size()))
        return false;
    fCompressed = vchPubKey.IsCompressed();
    fValid = true;

    if (fSkipCheck)
        return true;

    return VerifyPubKey(vchPubKey);
}

bool CKey::Derive(CKey& keyChild, ChainCode &ccChild, unsigned int nChild, const ChainCode& cc) const {
    assert(IsValid());
    assert(IsCompressed());
    std::vector<unsigned char, secure_allocator<unsigned char>> vout(64);
    if ((nChild >> 31) == 0) {
        CPubKey pubkey = GetPubKey();
        assert(pubkey.begin() + 33 == pubkey.end());
        BIP32Hash(cc, nChild, *pubkey.begin(), pubkey.begin()+1, vout.data());
    } else {
        assert(begin() + 32 == end());
        BIP32Hash(cc, nChild, 0, begin(), vout.data());
    }
    memcpy(ccChild.begin(), vout.data()+32, 32);
    memcpy((unsigned char*)keyChild.begin(), begin(), 32);
    bool ret = secp256k1_ec_privkey_tweak_add(secp256k1_context_sign, (unsigned char*)keyChild.begin(), vout.data());
    keyChild.fCompressed = true;
    keyChild.fValid = ret;
    return ret;
}

bool CExtKey::Derive(CExtKey &out, unsigned int _nChild) const {
    out.nDepth = nDepth + 1;
    CKeyID id = key.GetPubKey().GetID();
    memcpy(&out.vchFingerprint[0], &id, 4);
    out.nChild = _nChild;
    return key.Derive(out.key, out.chaincode, _nChild, chaincode);
}

void CExtKey::SetMaster(const unsigned char *seed, unsigned int nSeedLen) {
    static const unsigned char hashkey[] = {'B','i','t','c','o','i','n',' ','s','e','e','d'};
    std::vector<unsigned char, secure_allocator<unsigned char>> vout(64);
    CHMAC_SHA512(hashkey, sizeof(hashkey)).Write(seed, nSeedLen).Finalize(vout.data());
    key.Set(&vout[0], &vout[32], true);
    memcpy(chaincode.begin(), &vout[32], 32);
    nDepth = 0;
    nChild = 0;
    memset(vchFingerprint, 0, sizeof(vchFingerprint));
}

CExtPubKey CExtKey::Neuter() const {
    CExtPubKey ret;
    ret.nDepth = nDepth;
    memcpy(&ret.vchFingerprint[0], &vchFingerprint[0], 4);
    ret.nChild = nChild;
    ret.pubkey = key.GetPubKey();
    ret.chaincode = chaincode;
    return ret;
}

void CExtKey::Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const {
    code[0] = nDepth;
    memcpy(code+1, vchFingerprint, 4);
    code[5] = (nChild >> 24) & 0xFF; code[6] = (nChild >> 16) & 0xFF;
    code[7] = (nChild >>  8) & 0xFF; code[8] = (nChild >>  0) & 0xFF;
    memcpy(code+9, chaincode.begin(), 32);
    code[41] = 0;
    assert(key.size() == 32);
    memcpy(code+42, key.begin(), 32);
}

void CExtKey::Decode(const unsigned char code[BIP32_EXTKEY_SIZE]) {
    nDepth = code[0];
    memcpy(vchFingerprint, code+1, 4);
    nChild = (code[5] << 24) | (code[6] << 16) | (code[7] << 8) | code[8];
    memcpy(chaincode.begin(), code+9, 32);
    key.Set(code+42, code+BIP32_EXTKEY_SIZE, true);
}

bool ECC_InitSanityCheck() {
    CKey key;
    key.MakeNewKey(true);
    CPubKey pubkey = key.GetPubKey();
    return key.VerifyPubKey(pubkey);
}


// TODOGUANG - change names of these functions for transparency as they now init picnic
void ECC_Start() {

//int picnic_init(picnic_params_t params, const char* path, unsigned int flags);
	picnic_init(LowMC_256_256_10_38_UR,"pathtoprecomputedvalues",0); // 0 for init OpenSSL too, path is to the precomputed constants for LowMC boxes
}

void ECC_Stop() {

//void picnic_shutdown(unsigned int flags);
	picnic_shutdown(0); //0 for de-init OpenSSL too

}
