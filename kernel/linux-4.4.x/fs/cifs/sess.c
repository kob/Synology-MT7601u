#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include "cifspdu.h"
#include "cifsglob.h"
#include "cifsproto.h"
#include "cifs_unicode.h"
#include "cifs_debug.h"
#include "ntlmssp.h"
#include "nterr.h"
#include <linux/utsname.h>
#include <linux/slab.h>
#include "cifs_spnego.h"

static __u32 cifs_ssetup_hdr(struct cifs_ses *ses, SESSION_SETUP_ANDX *pSMB)
{
	__u32 capabilities = 0;

	pSMB->req.AndXCommand = 0xFF;
	pSMB->req.MaxBufferSize = cpu_to_le16(min_t(u32,
					CIFSMaxBufSize + MAX_CIFS_HDR_SIZE - 4,
					USHRT_MAX));
	pSMB->req.MaxMpxCount = cpu_to_le16(ses->server->maxReq);
	pSMB->req.VcNumber = cpu_to_le16(1);

	capabilities = CAP_LARGE_FILES | CAP_NT_SMBS | CAP_LEVEL_II_OPLOCKS |
			CAP_LARGE_WRITE_X | CAP_LARGE_READ_X;

	if (ses->server->sign)
		pSMB->req.hdr.Flags2 |= SMBFLG2_SECURITY_SIGNATURE;

	if (ses->capabilities & CAP_UNICODE) {
		pSMB->req.hdr.Flags2 |= SMBFLG2_UNICODE;
		capabilities |= CAP_UNICODE;
	}
	if (ses->capabilities & CAP_STATUS32) {
		pSMB->req.hdr.Flags2 |= SMBFLG2_ERR_STATUS;
		capabilities |= CAP_STATUS32;
	}
	if (ses->capabilities & CAP_DFS) {
		pSMB->req.hdr.Flags2 |= SMBFLG2_DFS;
		capabilities |= CAP_DFS;
	}
	if (ses->capabilities & CAP_UNIX)
		capabilities |= CAP_UNIX;

	return capabilities;
}

static void
unicode_oslm_strings(char **pbcc_area, const struct nls_table *nls_cp)
{
	char *bcc_ptr = *pbcc_area;
	int bytes_ret = 0;

#ifdef MY_ABC_HERE
	bytes_ret = cifs_strtoUTF16((__le16 *)bcc_ptr, "Synology Linux version ", 32,
				  nls_cp);
#else
	bytes_ret = cifs_strtoUTF16((__le16 *)bcc_ptr, "Linux version ", 32,
				    nls_cp);
#endif  
	bcc_ptr += 2 * bytes_ret;
	bytes_ret = cifs_strtoUTF16((__le16 *) bcc_ptr, init_utsname()->release,
				    32, nls_cp);
	bcc_ptr += 2 * bytes_ret;
	bcc_ptr += 2;  

	bytes_ret = cifs_strtoUTF16((__le16 *) bcc_ptr, CIFS_NETWORK_OPSYS,
				    32, nls_cp);
	bcc_ptr += 2 * bytes_ret;
	bcc_ptr += 2;  

	*pbcc_area = bcc_ptr;
}

static void unicode_domain_string(char **pbcc_area, struct cifs_ses *ses,
				   const struct nls_table *nls_cp)
{
	char *bcc_ptr = *pbcc_area;
	int bytes_ret = 0;

	if (ses->domainName == NULL) {
		 
		*bcc_ptr = 0;
		*(bcc_ptr+1) = 0;
		bytes_ret = 0;
	} else
		bytes_ret = cifs_strtoUTF16((__le16 *) bcc_ptr, ses->domainName,
					    CIFS_MAX_DOMAINNAME_LEN, nls_cp);
	bcc_ptr += 2 * bytes_ret;
	bcc_ptr += 2;   

	*pbcc_area = bcc_ptr;
}

static void unicode_ssetup_strings(char **pbcc_area, struct cifs_ses *ses,
				   const struct nls_table *nls_cp)
{
	char *bcc_ptr = *pbcc_area;
	int bytes_ret = 0;

	if (ses->user_name == NULL) {
		 
		*bcc_ptr = 0;
		*(bcc_ptr+1) = 0;
	} else {
		bytes_ret = cifs_strtoUTF16((__le16 *) bcc_ptr, ses->user_name,
					    CIFS_MAX_USERNAME_LEN, nls_cp);
	}
	bcc_ptr += 2 * bytes_ret;
	bcc_ptr += 2;  

	unicode_domain_string(&bcc_ptr, ses, nls_cp);
	unicode_oslm_strings(&bcc_ptr, nls_cp);

	*pbcc_area = bcc_ptr;
}

static void ascii_ssetup_strings(char **pbcc_area, struct cifs_ses *ses,
				 const struct nls_table *nls_cp)
{
	char *bcc_ptr = *pbcc_area;

	if (ses->user_name != NULL) {
		strncpy(bcc_ptr, ses->user_name, CIFS_MAX_USERNAME_LEN);
		bcc_ptr += strnlen(ses->user_name, CIFS_MAX_USERNAME_LEN);
	}
	 
	*bcc_ptr = 0;
	bcc_ptr++;  

	if (ses->domainName != NULL) {
		strncpy(bcc_ptr, ses->domainName, CIFS_MAX_DOMAINNAME_LEN);
		bcc_ptr += strnlen(ses->domainName, CIFS_MAX_DOMAINNAME_LEN);
	}  
	*bcc_ptr = 0;
	bcc_ptr++;

#ifdef MY_ABC_HERE
	strcpy(bcc_ptr, "Synology Linux version ");
	bcc_ptr += strlen("Synology Linux version ");
#else
	strcpy(bcc_ptr, "Linux version ");
	bcc_ptr += strlen("Linux version ");
#endif  
	strcpy(bcc_ptr, init_utsname()->release);
	bcc_ptr += strlen(init_utsname()->release) + 1;

	strcpy(bcc_ptr, CIFS_NETWORK_OPSYS);
	bcc_ptr += strlen(CIFS_NETWORK_OPSYS) + 1;

	*pbcc_area = bcc_ptr;
}

static void
decode_unicode_ssetup(char **pbcc_area, int bleft, struct cifs_ses *ses,
		      const struct nls_table *nls_cp)
{
	int len;
	char *data = *pbcc_area;

	cifs_dbg(FYI, "bleft %d\n", bleft);

	kfree(ses->serverOS);
	ses->serverOS = cifs_strndup_from_utf16(data, bleft, true, nls_cp);
	cifs_dbg(FYI, "serverOS=%s\n", ses->serverOS);
	len = (UniStrnlen((wchar_t *) data, bleft / 2) * 2) + 2;
	data += len;
	bleft -= len;
	if (bleft <= 0)
		return;

	kfree(ses->serverNOS);
	ses->serverNOS = cifs_strndup_from_utf16(data, bleft, true, nls_cp);
	cifs_dbg(FYI, "serverNOS=%s\n", ses->serverNOS);
	len = (UniStrnlen((wchar_t *) data, bleft / 2) * 2) + 2;
	data += len;
	bleft -= len;
	if (bleft <= 0)
		return;

	kfree(ses->serverDomain);
	ses->serverDomain = cifs_strndup_from_utf16(data, bleft, true, nls_cp);
	cifs_dbg(FYI, "serverDomain=%s\n", ses->serverDomain);

	return;
}

static void decode_ascii_ssetup(char **pbcc_area, __u16 bleft,
				struct cifs_ses *ses,
				const struct nls_table *nls_cp)
{
	int len;
	char *bcc_ptr = *pbcc_area;

	cifs_dbg(FYI, "decode sessetup ascii. bleft %d\n", bleft);

	len = strnlen(bcc_ptr, bleft);
	if (len >= bleft)
		return;

	kfree(ses->serverOS);

	ses->serverOS = kzalloc(len + 1, GFP_KERNEL);
	if (ses->serverOS) {
		strncpy(ses->serverOS, bcc_ptr, len);
		if (strncmp(ses->serverOS, "OS/2", 4) == 0)
			cifs_dbg(FYI, "OS/2 server\n");
	}

	bcc_ptr += len + 1;
	bleft -= len + 1;

	len = strnlen(bcc_ptr, bleft);
	if (len >= bleft)
		return;

	kfree(ses->serverNOS);

	ses->serverNOS = kzalloc(len + 1, GFP_KERNEL);
	if (ses->serverNOS)
		strncpy(ses->serverNOS, bcc_ptr, len);

	bcc_ptr += len + 1;
	bleft -= len + 1;

	len = strnlen(bcc_ptr, bleft);
	if (len > bleft)
		return;

	cifs_dbg(FYI, "ascii: bytes left %d\n", bleft);
}

int decode_ntlmssp_challenge(char *bcc_ptr, int blob_len,
				    struct cifs_ses *ses)
{
	unsigned int tioffset;  
	unsigned int tilen;  

	CHALLENGE_MESSAGE *pblob = (CHALLENGE_MESSAGE *)bcc_ptr;

	if (blob_len < sizeof(CHALLENGE_MESSAGE)) {
		cifs_dbg(VFS, "challenge blob len %d too small\n", blob_len);
		return -EINVAL;
	}

	if (memcmp(pblob->Signature, "NTLMSSP", 8)) {
		cifs_dbg(VFS, "blob signature incorrect %s\n",
			 pblob->Signature);
		return -EINVAL;
	}
	if (pblob->MessageType != NtLmChallenge) {
		cifs_dbg(VFS, "Incorrect message type %d\n",
			 pblob->MessageType);
		return -EINVAL;
	}

	memcpy(ses->ntlmssp->cryptkey, pblob->Challenge, CIFS_CRYPTO_KEY_SIZE);
	 
	ses->ntlmssp->server_flags = le32_to_cpu(pblob->NegotiateFlags);
	tioffset = le32_to_cpu(pblob->TargetInfoArray.BufferOffset);
	tilen = le16_to_cpu(pblob->TargetInfoArray.Length);
	if (tioffset > blob_len || tioffset + tilen > blob_len) {
		cifs_dbg(VFS, "tioffset + tilen too high %u + %u",
			tioffset, tilen);
		return -EINVAL;
	}
	if (tilen) {
		ses->auth_key.response = kmemdup(bcc_ptr + tioffset, tilen,
						 GFP_KERNEL);
		if (!ses->auth_key.response) {
			cifs_dbg(VFS, "Challenge target info alloc failure");
			return -ENOMEM;
		}
		ses->auth_key.len = tilen;
	}

	return 0;
}

void build_ntlmssp_negotiate_blob(unsigned char *pbuffer,
					 struct cifs_ses *ses)
{
	NEGOTIATE_MESSAGE *sec_blob = (NEGOTIATE_MESSAGE *)pbuffer;
	__u32 flags;

	memset(pbuffer, 0, sizeof(NEGOTIATE_MESSAGE));
	memcpy(sec_blob->Signature, NTLMSSP_SIGNATURE, 8);
	sec_blob->MessageType = NtLmNegotiate;

	flags = NTLMSSP_NEGOTIATE_56 |	NTLMSSP_REQUEST_TARGET |
		NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_UNICODE |
		NTLMSSP_NEGOTIATE_NTLM | NTLMSSP_NEGOTIATE_EXTENDED_SEC;
	if (ses->server->sign) {
		flags |= NTLMSSP_NEGOTIATE_SIGN;
		if (!ses->server->session_estab ||
				ses->ntlmssp->sesskey_per_smbsess)
			flags |= NTLMSSP_NEGOTIATE_KEY_XCH;
	}

	sec_blob->NegotiateFlags = cpu_to_le32(flags);

	sec_blob->WorkstationName.BufferOffset = 0;
	sec_blob->WorkstationName.Length = 0;
	sec_blob->WorkstationName.MaximumLength = 0;

	sec_blob->DomainName.BufferOffset = 0;
	sec_blob->DomainName.Length = 0;
	sec_blob->DomainName.MaximumLength = 0;
}

static int size_of_ntlmssp_blob(struct cifs_ses *ses)
{
	int sz = sizeof(AUTHENTICATE_MESSAGE) + ses->auth_key.len
		- CIFS_SESS_KEY_SIZE + CIFS_CPHTXT_SIZE + 2;

	if (ses->domainName)
		sz += 2 * strnlen(ses->domainName, CIFS_MAX_DOMAINNAME_LEN);
	else
		sz += 2;

	if (ses->user_name)
		sz += 2 * strnlen(ses->user_name, CIFS_MAX_USERNAME_LEN);
	else
		sz += 2;

	return sz;
}

int build_ntlmssp_auth_blob(unsigned char **pbuffer,
					u16 *buflen,
				   struct cifs_ses *ses,
				   const struct nls_table *nls_cp)
{
	int rc;
	AUTHENTICATE_MESSAGE *sec_blob;
	__u32 flags;
	unsigned char *tmp;

	rc = setup_ntlmv2_rsp(ses, nls_cp);
	if (rc) {
		cifs_dbg(VFS, "Error %d during NTLMSSP authentication\n", rc);
		*buflen = 0;
		goto setup_ntlmv2_ret;
	}
	*pbuffer = kmalloc(size_of_ntlmssp_blob(ses), GFP_KERNEL);
	sec_blob = (AUTHENTICATE_MESSAGE *)*pbuffer;

	memcpy(sec_blob->Signature, NTLMSSP_SIGNATURE, 8);
	sec_blob->MessageType = NtLmAuthenticate;

	flags = NTLMSSP_NEGOTIATE_56 |
		NTLMSSP_REQUEST_TARGET | NTLMSSP_NEGOTIATE_TARGET_INFO |
		NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_UNICODE |
		NTLMSSP_NEGOTIATE_NTLM | NTLMSSP_NEGOTIATE_EXTENDED_SEC;
	if (ses->server->sign) {
		flags |= NTLMSSP_NEGOTIATE_SIGN;
		if (!ses->server->session_estab ||
				ses->ntlmssp->sesskey_per_smbsess)
			flags |= NTLMSSP_NEGOTIATE_KEY_XCH;
	}

	tmp = *pbuffer + sizeof(AUTHENTICATE_MESSAGE);
	sec_blob->NegotiateFlags = cpu_to_le32(flags);

	sec_blob->LmChallengeResponse.BufferOffset =
				cpu_to_le32(sizeof(AUTHENTICATE_MESSAGE));
	sec_blob->LmChallengeResponse.Length = 0;
	sec_blob->LmChallengeResponse.MaximumLength = 0;

	sec_blob->NtChallengeResponse.BufferOffset =
				cpu_to_le32(tmp - *pbuffer);
	if (ses->user_name != NULL) {
		memcpy(tmp, ses->auth_key.response + CIFS_SESS_KEY_SIZE,
				ses->auth_key.len - CIFS_SESS_KEY_SIZE);
		tmp += ses->auth_key.len - CIFS_SESS_KEY_SIZE;

		sec_blob->NtChallengeResponse.Length =
				cpu_to_le16(ses->auth_key.len - CIFS_SESS_KEY_SIZE);
		sec_blob->NtChallengeResponse.MaximumLength =
				cpu_to_le16(ses->auth_key.len - CIFS_SESS_KEY_SIZE);
	} else {
		 
		sec_blob->NtChallengeResponse.Length = 0;
		sec_blob->NtChallengeResponse.MaximumLength = 0;
	}

	if (ses->domainName == NULL) {
		sec_blob->DomainName.BufferOffset = cpu_to_le32(tmp - *pbuffer);
		sec_blob->DomainName.Length = 0;
		sec_blob->DomainName.MaximumLength = 0;
		tmp += 2;
	} else {
		int len;
		len = cifs_strtoUTF16((__le16 *)tmp, ses->domainName,
				      CIFS_MAX_USERNAME_LEN, nls_cp);
		len *= 2;  
		sec_blob->DomainName.BufferOffset = cpu_to_le32(tmp - *pbuffer);
		sec_blob->DomainName.Length = cpu_to_le16(len);
		sec_blob->DomainName.MaximumLength = cpu_to_le16(len);
		tmp += len;
	}

	if (ses->user_name == NULL) {
		sec_blob->UserName.BufferOffset = cpu_to_le32(tmp - *pbuffer);
		sec_blob->UserName.Length = 0;
		sec_blob->UserName.MaximumLength = 0;
		tmp += 2;
	} else {
		int len;
		len = cifs_strtoUTF16((__le16 *)tmp, ses->user_name,
				      CIFS_MAX_USERNAME_LEN, nls_cp);
		len *= 2;  
		sec_blob->UserName.BufferOffset = cpu_to_le32(tmp - *pbuffer);
		sec_blob->UserName.Length = cpu_to_le16(len);
		sec_blob->UserName.MaximumLength = cpu_to_le16(len);
		tmp += len;
	}

	sec_blob->WorkstationName.BufferOffset = cpu_to_le32(tmp - *pbuffer);
	sec_blob->WorkstationName.Length = 0;
	sec_blob->WorkstationName.MaximumLength = 0;
	tmp += 2;

	if (((ses->ntlmssp->server_flags & NTLMSSP_NEGOTIATE_KEY_XCH) ||
		(ses->ntlmssp->server_flags & NTLMSSP_NEGOTIATE_EXTENDED_SEC))
			&& !calc_seckey(ses)) {
		memcpy(tmp, ses->ntlmssp->ciphertext, CIFS_CPHTXT_SIZE);
		sec_blob->SessionKey.BufferOffset = cpu_to_le32(tmp - *pbuffer);
		sec_blob->SessionKey.Length = cpu_to_le16(CIFS_CPHTXT_SIZE);
		sec_blob->SessionKey.MaximumLength =
				cpu_to_le16(CIFS_CPHTXT_SIZE);
		tmp += CIFS_CPHTXT_SIZE;
	} else {
		sec_blob->SessionKey.BufferOffset = cpu_to_le32(tmp - *pbuffer);
		sec_blob->SessionKey.Length = 0;
		sec_blob->SessionKey.MaximumLength = 0;
	}

	*buflen = tmp - *pbuffer;
setup_ntlmv2_ret:
	return rc;
}

enum securityEnum
select_sectype(struct TCP_Server_Info *server, enum securityEnum requested)
{
	switch (server->negflavor) {
	case CIFS_NEGFLAVOR_EXTENDED:
		switch (requested) {
		case Kerberos:
		case RawNTLMSSP:
			return requested;
		case Unspecified:
			if (server->sec_ntlmssp &&
			    (global_secflags & CIFSSEC_MAY_NTLMSSP))
				return RawNTLMSSP;
			if ((server->sec_kerberos || server->sec_mskerberos) &&
			    (global_secflags & CIFSSEC_MAY_KRB5))
				return Kerberos;
			 
		default:
			return Unspecified;
		}
	case CIFS_NEGFLAVOR_UNENCAP:
		switch (requested) {
		case NTLM:
		case NTLMv2:
			return requested;
		case Unspecified:
			if (global_secflags & CIFSSEC_MAY_NTLMV2)
				return NTLMv2;
			if (global_secflags & CIFSSEC_MAY_NTLM)
				return NTLM;
		default:
			 
			break;
		}
	case CIFS_NEGFLAVOR_LANMAN:
		switch (requested) {
		case LANMAN:
			return requested;
		case Unspecified:
			if (global_secflags & CIFSSEC_MAY_LANMAN)
				return LANMAN;
			 
		default:
			return Unspecified;
		}
	default:
		return Unspecified;
	}
}

struct sess_data {
	unsigned int xid;
	struct cifs_ses *ses;
	struct nls_table *nls_cp;
	void (*func)(struct sess_data *);
	int result;

	int buf0_type;
	struct kvec iov[3];
};

static int
sess_alloc_buffer(struct sess_data *sess_data, int wct)
{
	int rc;
	struct cifs_ses *ses = sess_data->ses;
	struct smb_hdr *smb_buf;

	rc = small_smb_init_no_tc(SMB_COM_SESSION_SETUP_ANDX, wct, ses,
				  (void **)&smb_buf);

	if (rc)
		return rc;

	sess_data->iov[0].iov_base = (char *)smb_buf;
	sess_data->iov[0].iov_len = be32_to_cpu(smb_buf->smb_buf_length) + 4;
	 
	sess_data->buf0_type = CIFS_SMALL_BUFFER;

	sess_data->iov[2].iov_base = kmalloc(2000, GFP_KERNEL);
	if (!sess_data->iov[2].iov_base) {
		rc = -ENOMEM;
		goto out_free_smb_buf;
	}

	return 0;

out_free_smb_buf:
	kfree(smb_buf);
	sess_data->iov[0].iov_base = NULL;
	sess_data->iov[0].iov_len = 0;
	sess_data->buf0_type = CIFS_NO_BUFFER;
	return rc;
}

static void
sess_free_buffer(struct sess_data *sess_data)
{

	free_rsp_buf(sess_data->buf0_type, sess_data->iov[0].iov_base);
	sess_data->buf0_type = CIFS_NO_BUFFER;
	kfree(sess_data->iov[2].iov_base);
}

static int
sess_establish_session(struct sess_data *sess_data)
{
	struct cifs_ses *ses = sess_data->ses;

	mutex_lock(&ses->server->srv_mutex);
	if (!ses->server->session_estab) {
		if (ses->server->sign) {
			ses->server->session_key.response =
				kmemdup(ses->auth_key.response,
				ses->auth_key.len, GFP_KERNEL);
			if (!ses->server->session_key.response) {
				mutex_unlock(&ses->server->srv_mutex);
				return -ENOMEM;
			}
			ses->server->session_key.len =
						ses->auth_key.len;
		}
		ses->server->sequence_number = 0x2;
		ses->server->session_estab = true;
	}
	mutex_unlock(&ses->server->srv_mutex);

	cifs_dbg(FYI, "CIFS session established successfully\n");
	spin_lock(&GlobalMid_Lock);
	ses->status = CifsGood;
	ses->need_reconnect = false;
	spin_unlock(&GlobalMid_Lock);

	return 0;
}

static int
sess_sendreceive(struct sess_data *sess_data)
{
	int rc;
	struct smb_hdr *smb_buf = (struct smb_hdr *) sess_data->iov[0].iov_base;
	__u16 count;

	count = sess_data->iov[1].iov_len + sess_data->iov[2].iov_len;
	smb_buf->smb_buf_length =
		cpu_to_be32(be32_to_cpu(smb_buf->smb_buf_length) + count);
	put_bcc(count, smb_buf);

	rc = SendReceive2(sess_data->xid, sess_data->ses,
			  sess_data->iov, 3  ,
			  &sess_data->buf0_type,
			  CIFS_LOG_ERROR);

	return rc;
}

#ifdef CONFIG_CIFS_WEAK_PW_HASH
static void
sess_auth_lanman(struct sess_data *sess_data)
{
	int rc = 0;
	struct smb_hdr *smb_buf;
	SESSION_SETUP_ANDX *pSMB;
	char *bcc_ptr;
	struct cifs_ses *ses = sess_data->ses;
	char lnm_session_key[CIFS_AUTH_RESP_SIZE];
	__u32 capabilities;
	__u16 bytes_remaining;

	rc = sess_alloc_buffer(sess_data, 10);
	if (rc)
		goto out;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	bcc_ptr = sess_data->iov[2].iov_base;
	capabilities = cifs_ssetup_hdr(ses, pSMB);

	pSMB->req.hdr.Flags2 &= ~SMBFLG2_UNICODE;

	if (ses->user_name != NULL) {
		 
		pSMB->old_req.PasswordLength = cpu_to_le16(CIFS_AUTH_RESP_SIZE);

		rc = calc_lanman_hash(ses->password, ses->server->cryptkey,
				      ses->server->sec_mode & SECMODE_PW_ENCRYPT ?
				      true : false, lnm_session_key);

		memcpy(bcc_ptr, (char *)lnm_session_key, CIFS_AUTH_RESP_SIZE);
		bcc_ptr += CIFS_AUTH_RESP_SIZE;
	} else {
		pSMB->old_req.PasswordLength = 0;
	}

	cifs_dbg(FYI, "Negotiating LANMAN setting up strings\n");
	 
	ascii_ssetup_strings(&bcc_ptr, ses, sess_data->nls_cp);

	sess_data->iov[2].iov_len = (long) bcc_ptr -
			(long) sess_data->iov[2].iov_base;

	rc = sess_sendreceive(sess_data);
	if (rc)
		goto out;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	smb_buf = (struct smb_hdr *)sess_data->iov[0].iov_base;

	if (smb_buf->WordCount != 3) {
		rc = -EIO;
		cifs_dbg(VFS, "bad word count %d\n", smb_buf->WordCount);
		goto out;
	}

	if (le16_to_cpu(pSMB->resp.Action) & GUEST_LOGIN)
		cifs_dbg(FYI, "Guest login\n");  

	ses->Suid = smb_buf->Uid;    
	cifs_dbg(FYI, "UID = %llu\n", ses->Suid);

	bytes_remaining = get_bcc(smb_buf);
	bcc_ptr = pByteArea(smb_buf);

	if (bytes_remaining == 0) {
		 
	} else if (smb_buf->Flags2 & SMBFLG2_UNICODE) {
		 
		if (((unsigned long) bcc_ptr - (unsigned long) smb_buf) % 2) {
			++bcc_ptr;
			--bytes_remaining;
		}
		decode_unicode_ssetup(&bcc_ptr, bytes_remaining, ses,
				      sess_data->nls_cp);
	} else {
		decode_ascii_ssetup(&bcc_ptr, bytes_remaining, ses,
				    sess_data->nls_cp);
	}

	rc = sess_establish_session(sess_data);
out:
	sess_data->result = rc;
	sess_data->func = NULL;
	sess_free_buffer(sess_data);
}

#endif

static void
sess_auth_ntlm(struct sess_data *sess_data)
{
	int rc = 0;
	struct smb_hdr *smb_buf;
	SESSION_SETUP_ANDX *pSMB;
	char *bcc_ptr;
	struct cifs_ses *ses = sess_data->ses;
	__u32 capabilities;
	__u16 bytes_remaining;

	rc = sess_alloc_buffer(sess_data, 13);
	if (rc)
		goto out;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	bcc_ptr = sess_data->iov[2].iov_base;
	capabilities = cifs_ssetup_hdr(ses, pSMB);

	pSMB->req_no_secext.Capabilities = cpu_to_le32(capabilities);
	if (ses->user_name != NULL) {
		pSMB->req_no_secext.CaseInsensitivePasswordLength =
				cpu_to_le16(CIFS_AUTH_RESP_SIZE);
		pSMB->req_no_secext.CaseSensitivePasswordLength =
				cpu_to_le16(CIFS_AUTH_RESP_SIZE);

		rc = setup_ntlm_response(ses, sess_data->nls_cp);
		if (rc) {
			cifs_dbg(VFS, "Error %d during NTLM authentication\n",
					 rc);
			goto out;
		}

		memcpy(bcc_ptr, ses->auth_key.response + CIFS_SESS_KEY_SIZE,
				CIFS_AUTH_RESP_SIZE);
		bcc_ptr += CIFS_AUTH_RESP_SIZE;
		memcpy(bcc_ptr, ses->auth_key.response + CIFS_SESS_KEY_SIZE,
				CIFS_AUTH_RESP_SIZE);
		bcc_ptr += CIFS_AUTH_RESP_SIZE;
	} else {
		pSMB->req_no_secext.CaseInsensitivePasswordLength = 0;
		pSMB->req_no_secext.CaseSensitivePasswordLength = 0;
	}

	if (ses->capabilities & CAP_UNICODE) {
		 
		if (sess_data->iov[0].iov_len % 2) {
			*bcc_ptr = 0;
			bcc_ptr++;
		}
		unicode_ssetup_strings(&bcc_ptr, ses, sess_data->nls_cp);
	} else {
		ascii_ssetup_strings(&bcc_ptr, ses, sess_data->nls_cp);
	}

	sess_data->iov[2].iov_len = (long) bcc_ptr -
			(long) sess_data->iov[2].iov_base;

	rc = sess_sendreceive(sess_data);
	if (rc)
		goto out;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	smb_buf = (struct smb_hdr *)sess_data->iov[0].iov_base;

	if (smb_buf->WordCount != 3) {
		rc = -EIO;
		cifs_dbg(VFS, "bad word count %d\n", smb_buf->WordCount);
		goto out;
	}

	if (le16_to_cpu(pSMB->resp.Action) & GUEST_LOGIN)
		cifs_dbg(FYI, "Guest login\n");  

	ses->Suid = smb_buf->Uid;    
	cifs_dbg(FYI, "UID = %llu\n", ses->Suid);

	bytes_remaining = get_bcc(smb_buf);
	bcc_ptr = pByteArea(smb_buf);

	if (bytes_remaining == 0) {
		 
	} else if (smb_buf->Flags2 & SMBFLG2_UNICODE) {
		 
		if (((unsigned long) bcc_ptr - (unsigned long) smb_buf) % 2) {
			++bcc_ptr;
			--bytes_remaining;
		}
		decode_unicode_ssetup(&bcc_ptr, bytes_remaining, ses,
				      sess_data->nls_cp);
	} else {
		decode_ascii_ssetup(&bcc_ptr, bytes_remaining, ses,
				    sess_data->nls_cp);
	}

	rc = sess_establish_session(sess_data);
out:
	sess_data->result = rc;
	sess_data->func = NULL;
	sess_free_buffer(sess_data);
	kfree(ses->auth_key.response);
	ses->auth_key.response = NULL;
}

static void
sess_auth_ntlmv2(struct sess_data *sess_data)
{
	int rc = 0;
	struct smb_hdr *smb_buf;
	SESSION_SETUP_ANDX *pSMB;
	char *bcc_ptr;
	struct cifs_ses *ses = sess_data->ses;
	__u32 capabilities;
	__u16 bytes_remaining;

	rc = sess_alloc_buffer(sess_data, 13);
	if (rc)
		goto out;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	bcc_ptr = sess_data->iov[2].iov_base;
	capabilities = cifs_ssetup_hdr(ses, pSMB);

	pSMB->req_no_secext.Capabilities = cpu_to_le32(capabilities);

	pSMB->req_no_secext.CaseInsensitivePasswordLength = 0;

	if (ses->user_name != NULL) {
		 
		rc = setup_ntlmv2_rsp(ses, sess_data->nls_cp);
		if (rc) {
			cifs_dbg(VFS, "Error %d during NTLMv2 authentication\n", rc);
			goto out;
		}

		memcpy(bcc_ptr, ses->auth_key.response + CIFS_SESS_KEY_SIZE,
				ses->auth_key.len - CIFS_SESS_KEY_SIZE);
		bcc_ptr += ses->auth_key.len - CIFS_SESS_KEY_SIZE;

		pSMB->req_no_secext.CaseSensitivePasswordLength =
			cpu_to_le16(ses->auth_key.len - CIFS_SESS_KEY_SIZE);
	} else {
		pSMB->req_no_secext.CaseSensitivePasswordLength = 0;
	}

	if (ses->capabilities & CAP_UNICODE) {
		if (sess_data->iov[0].iov_len % 2) {
			*bcc_ptr = 0;
			bcc_ptr++;
		}
		unicode_ssetup_strings(&bcc_ptr, ses, sess_data->nls_cp);
	} else {
		ascii_ssetup_strings(&bcc_ptr, ses, sess_data->nls_cp);
	}

	sess_data->iov[2].iov_len = (long) bcc_ptr -
			(long) sess_data->iov[2].iov_base;

	rc = sess_sendreceive(sess_data);
	if (rc)
		goto out;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	smb_buf = (struct smb_hdr *)sess_data->iov[0].iov_base;

	if (smb_buf->WordCount != 3) {
		rc = -EIO;
		cifs_dbg(VFS, "bad word count %d\n", smb_buf->WordCount);
		goto out;
	}

	if (le16_to_cpu(pSMB->resp.Action) & GUEST_LOGIN)
		cifs_dbg(FYI, "Guest login\n");  

	ses->Suid = smb_buf->Uid;    
	cifs_dbg(FYI, "UID = %llu\n", ses->Suid);

	bytes_remaining = get_bcc(smb_buf);
	bcc_ptr = pByteArea(smb_buf);

	if (bytes_remaining == 0) {
		 
	} else if (smb_buf->Flags2 & SMBFLG2_UNICODE) {
		 
		if (((unsigned long) bcc_ptr - (unsigned long) smb_buf) % 2) {
			++bcc_ptr;
			--bytes_remaining;
		}
		decode_unicode_ssetup(&bcc_ptr, bytes_remaining, ses,
				      sess_data->nls_cp);
	} else {
		decode_ascii_ssetup(&bcc_ptr, bytes_remaining, ses,
				    sess_data->nls_cp);
	}

	rc = sess_establish_session(sess_data);
out:
	sess_data->result = rc;
	sess_data->func = NULL;
	sess_free_buffer(sess_data);
	kfree(ses->auth_key.response);
	ses->auth_key.response = NULL;
}

#ifdef CONFIG_CIFS_UPCALL
static void
sess_auth_kerberos(struct sess_data *sess_data)
{
	int rc = 0;
	struct smb_hdr *smb_buf;
	SESSION_SETUP_ANDX *pSMB;
	char *bcc_ptr;
	struct cifs_ses *ses = sess_data->ses;
	__u32 capabilities;
	__u16 bytes_remaining;
	struct key *spnego_key = NULL;
	struct cifs_spnego_msg *msg;
	u16 blob_len;

	rc = sess_alloc_buffer(sess_data, 12);
	if (rc)
		goto out;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	bcc_ptr = sess_data->iov[2].iov_base;
	capabilities = cifs_ssetup_hdr(ses, pSMB);

	spnego_key = cifs_get_spnego_key(ses);
	if (IS_ERR(spnego_key)) {
		rc = PTR_ERR(spnego_key);
		spnego_key = NULL;
		goto out;
	}

	msg = spnego_key->payload.data[0];
	 
	if (msg->version != CIFS_SPNEGO_UPCALL_VERSION) {
		cifs_dbg(VFS,
		  "incorrect version of cifs.upcall (expected %d but got %d)",
			      CIFS_SPNEGO_UPCALL_VERSION, msg->version);
		rc = -EKEYREJECTED;
		goto out_put_spnego_key;
	}

	ses->auth_key.response = kmemdup(msg->data, msg->sesskey_len,
					 GFP_KERNEL);
	if (!ses->auth_key.response) {
		cifs_dbg(VFS, "Kerberos can't allocate (%u bytes) memory",
				msg->sesskey_len);
		rc = -ENOMEM;
		goto out_put_spnego_key;
	}
	ses->auth_key.len = msg->sesskey_len;

	pSMB->req.hdr.Flags2 |= SMBFLG2_EXT_SEC;
	capabilities |= CAP_EXTENDED_SECURITY;
	pSMB->req.Capabilities = cpu_to_le32(capabilities);
	sess_data->iov[1].iov_base = msg->data + msg->sesskey_len;
	sess_data->iov[1].iov_len = msg->secblob_len;
	pSMB->req.SecurityBlobLength = cpu_to_le16(sess_data->iov[1].iov_len);

	if (ses->capabilities & CAP_UNICODE) {
		 
		if ((sess_data->iov[0].iov_len
			+ sess_data->iov[1].iov_len) % 2) {
			*bcc_ptr = 0;
			bcc_ptr++;
		}
		unicode_oslm_strings(&bcc_ptr, sess_data->nls_cp);
		unicode_domain_string(&bcc_ptr, ses, sess_data->nls_cp);
	} else {
		 
		ascii_ssetup_strings(&bcc_ptr, ses, sess_data->nls_cp);
	}

	sess_data->iov[2].iov_len = (long) bcc_ptr -
			(long) sess_data->iov[2].iov_base;

	rc = sess_sendreceive(sess_data);
	if (rc)
		goto out_put_spnego_key;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	smb_buf = (struct smb_hdr *)sess_data->iov[0].iov_base;

	if (smb_buf->WordCount != 4) {
		rc = -EIO;
		cifs_dbg(VFS, "bad word count %d\n", smb_buf->WordCount);
		goto out_put_spnego_key;
	}

	if (le16_to_cpu(pSMB->resp.Action) & GUEST_LOGIN)
		cifs_dbg(FYI, "Guest login\n");  

	ses->Suid = smb_buf->Uid;    
	cifs_dbg(FYI, "UID = %llu\n", ses->Suid);

	bytes_remaining = get_bcc(smb_buf);
	bcc_ptr = pByteArea(smb_buf);

	blob_len = le16_to_cpu(pSMB->resp.SecurityBlobLength);
	if (blob_len > bytes_remaining) {
		cifs_dbg(VFS, "bad security blob length %d\n",
				blob_len);
		rc = -EINVAL;
		goto out_put_spnego_key;
	}
	bcc_ptr += blob_len;
	bytes_remaining -= blob_len;

	if (bytes_remaining == 0) {
		 
	} else if (smb_buf->Flags2 & SMBFLG2_UNICODE) {
		 
		if (((unsigned long) bcc_ptr - (unsigned long) smb_buf) % 2) {
			++bcc_ptr;
			--bytes_remaining;
		}
		decode_unicode_ssetup(&bcc_ptr, bytes_remaining, ses,
				      sess_data->nls_cp);
	} else {
		decode_ascii_ssetup(&bcc_ptr, bytes_remaining, ses,
				    sess_data->nls_cp);
	}

	rc = sess_establish_session(sess_data);
out_put_spnego_key:
	key_invalidate(spnego_key);
	key_put(spnego_key);
out:
	sess_data->result = rc;
	sess_data->func = NULL;
	sess_free_buffer(sess_data);
	kfree(ses->auth_key.response);
	ses->auth_key.response = NULL;
}

#endif  

static int
_sess_auth_rawntlmssp_assemble_req(struct sess_data *sess_data)
{
	struct smb_hdr *smb_buf;
	SESSION_SETUP_ANDX *pSMB;
	struct cifs_ses *ses = sess_data->ses;
	__u32 capabilities;
	char *bcc_ptr;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	smb_buf = (struct smb_hdr *)pSMB;

	capabilities = cifs_ssetup_hdr(ses, pSMB);
	if ((pSMB->req.hdr.Flags2 & SMBFLG2_UNICODE) == 0) {
		cifs_dbg(VFS, "NTLMSSP requires Unicode support\n");
		return -ENOSYS;
	}

	pSMB->req.hdr.Flags2 |= SMBFLG2_EXT_SEC;
	capabilities |= CAP_EXTENDED_SECURITY;
	pSMB->req.Capabilities |= cpu_to_le32(capabilities);

	bcc_ptr = sess_data->iov[2].iov_base;
	 
	if ((sess_data->iov[0].iov_len + sess_data->iov[1].iov_len) % 2) {
		*bcc_ptr = 0;
		bcc_ptr++;
	}
	unicode_oslm_strings(&bcc_ptr, sess_data->nls_cp);

	sess_data->iov[2].iov_len = (long) bcc_ptr -
					(long) sess_data->iov[2].iov_base;

	return 0;
}

static void
sess_auth_rawntlmssp_authenticate(struct sess_data *sess_data);

static void
sess_auth_rawntlmssp_negotiate(struct sess_data *sess_data)
{
	int rc;
	struct smb_hdr *smb_buf;
	SESSION_SETUP_ANDX *pSMB;
	struct cifs_ses *ses = sess_data->ses;
	__u16 bytes_remaining;
	char *bcc_ptr;
	u16 blob_len;

	cifs_dbg(FYI, "rawntlmssp session setup negotiate phase\n");

	ses->ntlmssp = kmalloc(sizeof(struct ntlmssp_auth), GFP_KERNEL);
	if (!ses->ntlmssp) {
		rc = -ENOMEM;
		goto out;
	}
	ses->ntlmssp->sesskey_per_smbsess = false;

	rc = sess_alloc_buffer(sess_data, 12);
	if (rc)
		goto out;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;

	build_ntlmssp_negotiate_blob(pSMB->req.SecurityBlob, ses);
	sess_data->iov[1].iov_len = sizeof(NEGOTIATE_MESSAGE);
	sess_data->iov[1].iov_base = pSMB->req.SecurityBlob;
	pSMB->req.SecurityBlobLength = cpu_to_le16(sizeof(NEGOTIATE_MESSAGE));

	rc = _sess_auth_rawntlmssp_assemble_req(sess_data);
	if (rc)
		goto out;

	rc = sess_sendreceive(sess_data);

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	smb_buf = (struct smb_hdr *)sess_data->iov[0].iov_base;

	if (sess_data->buf0_type != CIFS_NO_BUFFER &&
	    smb_buf->Status.CifsError ==
			cpu_to_le32(NT_STATUS_MORE_PROCESSING_REQUIRED))
		rc = 0;

	if (rc)
		goto out;

	cifs_dbg(FYI, "rawntlmssp session setup challenge phase\n");

	if (smb_buf->WordCount != 4) {
		rc = -EIO;
		cifs_dbg(VFS, "bad word count %d\n", smb_buf->WordCount);
		goto out;
	}

	ses->Suid = smb_buf->Uid;    
	cifs_dbg(FYI, "UID = %llu\n", ses->Suid);

	bytes_remaining = get_bcc(smb_buf);
	bcc_ptr = pByteArea(smb_buf);

	blob_len = le16_to_cpu(pSMB->resp.SecurityBlobLength);
	if (blob_len > bytes_remaining) {
		cifs_dbg(VFS, "bad security blob length %d\n",
				blob_len);
		rc = -EINVAL;
		goto out;
	}

	rc = decode_ntlmssp_challenge(bcc_ptr, blob_len, ses);
out:
	sess_free_buffer(sess_data);

	if (!rc) {
		sess_data->func = sess_auth_rawntlmssp_authenticate;
		return;
	}

	kfree(ses->auth_key.response);
	ses->auth_key.response = NULL;
	kfree(ses->ntlmssp);
	ses->ntlmssp = NULL;

	sess_data->func = NULL;
	sess_data->result = rc;
}

static void
sess_auth_rawntlmssp_authenticate(struct sess_data *sess_data)
{
	int rc;
	struct smb_hdr *smb_buf;
	SESSION_SETUP_ANDX *pSMB;
	struct cifs_ses *ses = sess_data->ses;
	__u16 bytes_remaining;
	char *bcc_ptr;
	unsigned char *ntlmsspblob = NULL;
	u16 blob_len;

	cifs_dbg(FYI, "rawntlmssp session setup authenticate phase\n");

	rc = sess_alloc_buffer(sess_data, 12);
	if (rc)
		goto out;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	smb_buf = (struct smb_hdr *)pSMB;
	rc = build_ntlmssp_auth_blob(&ntlmsspblob,
					&blob_len, ses, sess_data->nls_cp);
	if (rc)
		goto out_free_ntlmsspblob;
	sess_data->iov[1].iov_len = blob_len;
	sess_data->iov[1].iov_base = ntlmsspblob;
	pSMB->req.SecurityBlobLength = cpu_to_le16(blob_len);
	 
	smb_buf->Uid = ses->Suid;

	rc = _sess_auth_rawntlmssp_assemble_req(sess_data);
	if (rc)
		goto out_free_ntlmsspblob;

	rc = sess_sendreceive(sess_data);
	if (rc)
		goto out_free_ntlmsspblob;

	pSMB = (SESSION_SETUP_ANDX *)sess_data->iov[0].iov_base;
	smb_buf = (struct smb_hdr *)sess_data->iov[0].iov_base;
	if (smb_buf->WordCount != 4) {
		rc = -EIO;
		cifs_dbg(VFS, "bad word count %d\n", smb_buf->WordCount);
		goto out_free_ntlmsspblob;
	}

	if (le16_to_cpu(pSMB->resp.Action) & GUEST_LOGIN)
		cifs_dbg(FYI, "Guest login\n");  

	if (ses->Suid != smb_buf->Uid) {
		ses->Suid = smb_buf->Uid;
		cifs_dbg(FYI, "UID changed! new UID = %llu\n", ses->Suid);
	}

	bytes_remaining = get_bcc(smb_buf);
	bcc_ptr = pByteArea(smb_buf);
	blob_len = le16_to_cpu(pSMB->resp.SecurityBlobLength);
	if (blob_len > bytes_remaining) {
		cifs_dbg(VFS, "bad security blob length %d\n",
				blob_len);
		rc = -EINVAL;
		goto out_free_ntlmsspblob;
	}
	bcc_ptr += blob_len;
	bytes_remaining -= blob_len;

	if (bytes_remaining == 0) {
		 
	} else if (smb_buf->Flags2 & SMBFLG2_UNICODE) {
		 
		if (((unsigned long) bcc_ptr - (unsigned long) smb_buf) % 2) {
			++bcc_ptr;
			--bytes_remaining;
		}
		decode_unicode_ssetup(&bcc_ptr, bytes_remaining, ses,
				      sess_data->nls_cp);
	} else {
		decode_ascii_ssetup(&bcc_ptr, bytes_remaining, ses,
				    sess_data->nls_cp);
	}

out_free_ntlmsspblob:
	kfree(ntlmsspblob);
out:
	sess_free_buffer(sess_data);

	 if (!rc)
		rc = sess_establish_session(sess_data);

	kfree(ses->auth_key.response);
	ses->auth_key.response = NULL;
	kfree(ses->ntlmssp);
	ses->ntlmssp = NULL;

	sess_data->func = NULL;
	sess_data->result = rc;
}

static int select_sec(struct cifs_ses *ses, struct sess_data *sess_data)
{
	int type;

	type = select_sectype(ses->server, ses->sectype);
	cifs_dbg(FYI, "sess setup type %d\n", type);
	if (type == Unspecified) {
		cifs_dbg(VFS,
			"Unable to select appropriate authentication method!");
		return -EINVAL;
	}

	switch (type) {
	case LANMAN:
		 
#ifdef CONFIG_CIFS_WEAK_PW_HASH
		sess_data->func = sess_auth_lanman;
		break;
#else
		return -EOPNOTSUPP;
#endif
	case NTLM:
		sess_data->func = sess_auth_ntlm;
		break;
	case NTLMv2:
		sess_data->func = sess_auth_ntlmv2;
		break;
	case Kerberos:
#ifdef CONFIG_CIFS_UPCALL
		sess_data->func = sess_auth_kerberos;
		break;
#else
		cifs_dbg(VFS, "Kerberos negotiated but upcall support disabled!\n");
		return -ENOSYS;
		break;
#endif  
	case RawNTLMSSP:
		sess_data->func = sess_auth_rawntlmssp_negotiate;
		break;
	default:
		cifs_dbg(VFS, "secType %d not supported!\n", type);
		return -ENOSYS;
	}

	return 0;
}

int CIFS_SessSetup(const unsigned int xid, struct cifs_ses *ses,
		    const struct nls_table *nls_cp)
{
	int rc = 0;
	struct sess_data *sess_data;

	if (ses == NULL) {
		WARN(1, "%s: ses == NULL!", __func__);
		return -EINVAL;
	}

	sess_data = kzalloc(sizeof(struct sess_data), GFP_KERNEL);
	if (!sess_data)
		return -ENOMEM;

	rc = select_sec(ses, sess_data);
	if (rc)
		goto out;

	sess_data->xid = xid;
	sess_data->ses = ses;
	sess_data->buf0_type = CIFS_NO_BUFFER;
	sess_data->nls_cp = (struct nls_table *) nls_cp;

	while (sess_data->func)
		sess_data->func(sess_data);

	rc = sess_data->result;

out:
	kfree(sess_data);
	return rc;
}