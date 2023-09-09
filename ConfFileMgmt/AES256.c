#include "AES256.h"

bool IsUsingOtherKeySet = true;
unsigned int GenerateSeqCodeForAES(void);

//key set 1
const unsigned int KeyBox[Nk]=
 {
 0x4F37AD24,0x3EA795DC,0x76e4a3b,0x7A46CED6,
 0xA87E5F4B,0x1C456BA6,0xA746D5C4,0xA69d34E7 
 };

//key set 2
const unsigned int KeyBox2[Nk]=
 {
 0x351DA345,0xA896FEC2,0x7BA3590D,0x5BC32544,
 0x94A6D158,0x9B10ADF5,0x3FE65A8B,0x6418ADBE
 };	

/*------------------------------
行移位
Shift operation of the state.
 
@state[4*4]
 
Return:
	0	OK
	<0	Fails
-------------------------------*/
int aes_ShiftRows(uint8_t *state)
{
	int j,k;
	uint8_t tmp;
 
	if(state==NULL)
		return -1;
 
	for(k=0; k<4; k++) {
		/* each row shift k times */
		for(j=0; j<k; j++) {
			tmp=*(state+4*k);   /* save the first byte */
			//memcpy(state+4*k, state+4*k+1, 3);
			memmove(state+4*k, state+4*k+1, 3);
			*(state+4*k+3)=tmp; /* set the last byte */
		}
	}
	return 0;
}
 
/*------------------------------
行逆移位
Shift operation of the state.
 
@state[4*4]
 
Return:
	0	OK
	<0	Fails
-------------------------------*/
int aes_InvShiftRows(uint8_t *state)
{
	int j,k;
	uint8_t tmp;
 
	if(state==NULL)
		return -1;
 
	for(k=0; k<4; k++) {
		/* each row shift k times */
		for(j=0; j<k; j++) {
			tmp=*(state+4*k+3); /* save the last byte */
			memmove(state+4*k+1, state+4*k, 3);
			*(state+4*k)=tmp;   /* set the first byte */
		}
	}
	return 0;
}
 
 
/*-------------------------------------------------------------
加植密钥
Add round key to the state.
 
@Nr:	     	Number of rounds, 10/12/14 for AES-128/192/256
@Nk:        	Key size, in words.
@round:	Current round number.
@state:	Pointer to state.
@keywords[Nb*(Nr+1)]:   All round keys, in words.
 
--------------------------------------------------------------*/
int aes_AddRoundKey(uint8_t round, uint8_t *state, const uint32_t *keywords)
{
	int k;
	if(state==NULL || keywords==NULL)
		return -1;
 
	for(k=0; k<4*4; k++)
		state[k] = ( keywords[round*4+k%4]>>((3-(k>>2))<<3) &0xFF )^state[k];
	return 0;
}
/*----------------------------------------------------------------------------------------------------------
密钥扩展
从输入密钥(也称为种子密码)扩展出Nr(10/12/14)个密钥，总共是Ｎr+1个密钥
Generate round keys.
 
@Nr:	    Number of rounds, 10/12/14 for AES-128/192/256
@Nk:        		Key size, in words.
@inkey[4*Nk]:  	        Original key, 4*Nk bytes, arranged row by row.
@keywords[Nb*(Nr+1)]:   Output keys, in words.  Nb*(Nr+1)
			one keywords(32 bytes) as one column of key_bytes(4 bytes)
 
Note:
1. The caller MUST ensure enough mem space of input params.
 
Return:
	0	Ok
	<0	Fails
---------------------------------------------------------------------------------------------------------------*/
int aes_ExpRoundKeys(uint32_t *keywords)
{
	int i;
	uint32_t temp;
 
	if(keywords==NULL)
		return -1;
 /* 从密钥盒里面数值*/
	if(IsUsingOtherKeySet==false)for(i=0;i<Nk;i++)
	  {
		keywords[i]=KeyBox[i];
		keywords[i]^=GenerateSeqCodeForAES();	//直接从KeyBox里面取内容，如果是Xmodem,则XOR数据包号生成的序列码
		}
	else for( i=0; i<Nk; i++ ) 
		keywords[i]=KeyBox2[i]^0x351AFE4D;
	/* Expend round keys */
	for(i=Nk; i<Nb*(Nr+1); i++) {
		temp=keywords[i-1];
		if( i%Nk==0 ) {
			/* RotWord */
			temp=( temp<<8 )+( temp>>24 );
			/* Subword */
			temp=(sbox[temp>>24]<<24) +(sbox[(temp>>16)&0xFF]<<16) +(sbox[(temp>>8)&0xFF]<<8)
				+sbox[temp&0xFF];
			/* temp=SubWord(RotWord(temp)) XOR Rcon[i/Nk-1] */
			temp=temp ^ Rcon[i/Nk-1];
		}
		else if (Nk>6 && i%Nk==4 ) {
			/* Subword */
			temp=(sbox[temp>>24]<<24) +(sbox[(temp>>16)&0xFF]<<16) +(sbox[(temp>>8)&0xFF]<<8)
                                +sbox[temp&0xFF];
		}
 
		/* Get keywords[i] */
		keywords[i]=keywords[i-Nk]^temp;
	}
	return 0;
}
/*----------------------------------------------------------------------
数据分组加密
Encrypt state.
 
@Nr:	   		 Number of rounds, 10/12/14 for AES-128/192/256
@Nk:        		 Key length, in words.
@keywordss[Nb*(Nr+1)]:   All round keys, in words.
@state[4*4]:		The state block.
 
Note:
1. The caller MUST ensure enough mem space of input params.
 
Return:
	0	Ok
	<0	Fails
------------------------------------------------------------------------*/
int aes_EncryptState(uint32_t *keywords, uint8_t *state)
{
	int i,k;
	uint8_t round;
	uint8_t mc[4];		    /* Temp. var */
 
	if(keywords==NULL || state==NULL)
		return -1;
        aes_AddRoundKey(0, state, keywords);
	 /* 循环Nr-1轮加密运算　Run Nr round functions */
	 for( round=1; round<Nr; round++) {  /* Nr */
 
		/* 2. SubBytes: 字节代换　Substitue State Bytes with SBOX */
		for(k=0; k<16; k++)
			state[k]=sbox[state[k]];
		/* 3. ShiftRow: 行移位　Shift State Rows */
		aes_ShiftRows(state);
		/* 4. MixColumn: 列混合　Mix State Cloumns */
		/* Galois Field Multiplication, Multi_Matrix:
			2 3 1 1
			1 2 3 1
			1 1 2 3
			3 1 1 2
		   Note:
		   1. Any number multiplied by 1 is equal to the number itself.
		   2. Any number multiplied by 0 is 0!
		*/
		for(i=0; i<4; i++) { /* i as column index */
	   		mc[0]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[2])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[3])%0xFF] )
				^state[i+8]^state[i+12];
			mc[1]=  state[i]
				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[2])%0xFF] )
				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[3])%0xFF] )
				^state[i+12];
			mc[2]=  state[i]^state[i+4]
				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[2])%0xFF] )
				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[3])%0xFF] );
			mc[3]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[3])%0xFF] )
				^state[i+4]^state[i+8]
				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[2])%0xFF] );
 
			state[i+0]=mc[0];
			state[i+4]=mc[1];
			state[i+8]=mc[2];
			state[i+12]=mc[3];
		}
		/* 5. AddRoundKey:  加植密钥　Add State with Round Key */
	  aes_AddRoundKey(round, state, keywords);
 
   	} /* END Nr rounds */
	/* 6. SubBytes: 字节代换　Substitue State Bytes with SBOX */
	for(k=0; k<16; k++)
		state[k]=sbox[state[k]];
	/* 7. ShiftRow: 行移位　Shift State Rows */
	aes_ShiftRows(state);
	/* 8. AddRoundKey:  加植密钥　Add State with Round Key */
        aes_AddRoundKey(round, state, keywords);
	return 0;
}
/*----------------------------------------------------------------------
Decrypt the state.
 
@Nr:	   		 Number of rounds, 10/12/14 for AES-128/192/256
@Nk:        		Key length, in words.
@keywordss[Nb*(Nr+1)]:  All round keys, in words.
@state[4*4]:		The state block.
 
Note:
1. The caller MUST ensure enough mem space of input params.
 
Return:
	0	Ok
	<0	Fails
------------------------------------------------------------------------*/
int aes_DecryptState(uint32_t *keywords, uint8_t *state)
{
	int i,k;
	uint8_t round;
	uint8_t mc[4];		    /* Temp. var */
 
	if(keywords==NULL || state==NULL)
		return -1;
 
	/* 1. AddRoundKey:  加植密钥　Add round key */
    aes_AddRoundKey(Nr, state, keywords);  /* From Nr_th round */
 
	 /* 循环Nr-1轮加密运算　Run Nr round functions */
	 for( round=Nr-1; round>0; round--) {  /* round [Nr-1  1]  */
		/* 2. InvShiftRow: 行逆移位　InvShift State Rows */
		aes_InvShiftRows(state);
		/* 3. InvSubBytes: 字节逆代换　InvSubstitue State Bytes with R_SBOX */
		for(k=0; k<16; k++)
			state[k]=rsbox[state[k]];
 
		/* 4. AddRoundKey:  加植密钥　Add State with Round Key */
	  aes_AddRoundKey(round, state, keywords);
 
		/* 5. InvMixColumn: 列逆混合　Inverse Mix State Cloumns */
		/* Galois Field Multiplication, Multi_Matrix:
			0x0E 0x0B 0x0D 0x09
			0x09 0x0E 0x0B 0x0D
			0x0D 0x09 0x0E 0x0B
			0x0B 0x0D 0x09 0x0E
		   Note:
		   1. Any number multiplied by 1 is equal to the number itself.
		   2. Any number multiplied by 0 is 0!
		*/
		for(i=0; i<4; i++) { 	/* i as column index */
	   		mc[0]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[0x0E])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[0x0B])%0xFF] )
  				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[0x0D])%0xFF] )
  				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[0x09])%0xFF] );
	   		mc[1]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[0x09])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[0x0E])%0xFF] )
  				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[0x0B])%0xFF] )
  				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[0x0D])%0xFF] );
	   		mc[2]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[0x0D])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[0x09])%0xFF] )
  				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[0x0E])%0xFF] )
  				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[0x0B])%0xFF] );
	   		mc[3]=  ( state[i]==0 ? 0 : Etab[(Ltab[state[i]]+Ltab[0x0B])%0xFF] )
  				^( state[i+4]==0 ? 0 : Etab[(Ltab[state[i+4]]+Ltab[0x0D])%0xFF] )
  				^( state[i+8]==0 ? 0 : Etab[(Ltab[state[i+8]]+Ltab[0x09])%0xFF] )
  				^( state[i+12]==0 ? 0 : Etab[(Ltab[state[i+12]]+Ltab[0x0E])%0xFF] );
 
			state[i+0]=mc[0];
			state[i+4]=mc[1];
			state[i+8]=mc[2];
			state[i+12]=mc[3];
		}
   	} /* END Nr rounds */
 
	/* 6. InvShiftRow: 行逆移位　Inverse Shift State Rows */
	aes_InvShiftRows(state);
	/* 7. InvSubBytes: 字节逆代换 InvSubstitue State Bytes with SBOX */
	for(k=0; k<16; k++)
		state[k]=rsbox[state[k]];
	/* 8. AddRoundKey:  加植密钥　Add State with Round Key */
        aes_AddRoundKey(0, state, keywords);
	return 0;
}
//AES加密或者解密数据（16 Byte,1=加密，0=解密）
int AES_EncryptDecryptData(char *InputBuf,char Mode)
{
	char k;
  uint8_t state[4*4];	/* 分组数据　State array, data in row sequence! */
  uint32_t keywords[Nb*(Nr+1)];  /* 用于存放扩展密钥，总共Nr+1把密钥　*/
  	/* 从输入密钥产生Nk+1个轮密，每轮需要一个密钥　Generate round keys */
	if(aes_ExpRoundKeys(keywords))return -1;
	/* 将待加密数据放入到数据分组state[]中，注意：state[]中数据按column顺序存放!　*/
	memset(state,0,sizeof(state));
	for(k=0; k<16; k++)
	state[(k%4)*4+k/4]=InputBuf[k];
  if(!Mode)//根据模式选择解密或者加密数据
	  {
	  if(aes_DecryptState(keywords,state))return -1;
    }
	else 
	  {
		if(aes_EncryptState(keywords,state))return -1;//加密数据
		}
	/*完成加密后从分组内取出数据*/
	for(k=0; k<16; k++)
	InputBuf[k]=state[(k%4)*4+k/4];
	return 0;
}
