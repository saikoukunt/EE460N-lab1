/*
	Name 1: Sai Susheel Koukuntla
	UTEID 1: sk46579
*/

#include <stdio.h> /* standard input/output library */
#include <stdlib.h> /* Standard C Library */
#include <string.h> /* String operations library */
#include <ctype.h> /* Library for useful character operations */
#include <limits.h> /* Library for definitions of common variable type characteristics */

//D E F I N I T I O N S
#define NUM_OPCODES 33
#define MAX_LINE_LENGTH 255
#define MAX_LABEL_LEN 20
#define MAX_SYMBOLS 255

#define imm5(x) (x < 0) ? x + 32 : x 
#define off6(x) (x < 0) ? x + 64 : x
#define off9(x) (x < 0) ? x + 512 : x
#define off11(x) (x < 0) ? x + 2048: x
#define imm16(x) (x < 0) ? x + 65536: x

typedef struct {
	int address;
	char label[MAX_LABEL_LEN + 1];	//Add 1 for null termination
} TableEntry;

enum {
    DONE, OK, EMPTY_LINE
};

char * opcodes[33] = {"add", "and", "br", "brp", "brz", "brzp", "brn", "brnp", "brnz",
"brnzp", "jmp", "ret", "jsr", "jsrr", "ldb", "ldw", "lea", "nop", "not", "rti", "lshf", "rshfa", 
"rshfl", "stw", "stb", "trap", "xor", "halt", "in", "out", "getc", "puts", ".fill" 
};

char * registers[8] = {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7"};

//G L O B A L S
FILE* infile = NULL;
FILE* outfile = NULL;
TableEntry symbolTable[MAX_SYMBOLS];
int symTabLength;

//P R O T O T Y P E S
int toNum(char * pStr);

int readAndParse( FILE * pInfile, char * pLine, char ** pLabel, char ** pOpcode, 
    char ** pArg1, char ** pArg2, char ** pArg3, char ** pArg4);

int isOpcode(char * opcode);

int addSymbol(char * label, int addr);

int convert(char * opcode, char * arg1, char * arg2, char * arg3, char * arg4, int lNum, int currAddr);

int isReg(char * lArg);

int isLabel(char * lArg, int lNum);

//F U N C T I O N S
int main(int argc, char* argv[]) {
    char lLine[MAX_LINE_LENGTH + 1], *lLabel, *lOpcode, *lArg1,
        *lArg2, *lArg3, *lArg4;
    char *prgName   = NULL;
    char *iFileName = NULL;
    char *oFileName = NULL;
    int lRet;
    symTabLength = 0;

    if(argc != 3){
        printf("Error: Incorrect number of arguments! Usage: assemble <source.asm> <output.obj>\n");
        exit(4);
    }

    prgName   = argv[0];
    iFileName = argv[1];
    oFileName = argv[2];

    //Open source and output files
    infile = fopen(argv[1], "r");
    outfile = fopen(argv[2], "w");
                
    if (!infile) {
      printf("Error: Cannot open file %s\n", argv[1]);
      exit(4);
    }
    if (!outfile) {
      printf("Error: Cannot open file %s\n", argv[2]);
      exit(4);
    }

    int lNum = 0;
   //Find first line, ignoring whitespace and comment-only lines
    do{
      lRet = readAndParse(infile, lLine, &lLabel,
                  &lOpcode, &lArg1, &lArg2, &lArg3, &lArg4 );
      lNum++;
    }
    while(strcmp(lLabel, "") == 0 && strcmp(lOpcode, "") ==  0);

    //_______________________________________________ .ORIG handling _________________________________________________________________
    if(strcmp(lOpcode, ".orig") != 0){
      printf("Error: file does not start with .ORIG\n");
      exit(4);
    }
    if(strcmp(lLabel, "") != 0){
      printf("Error: cannot label .ORIG (line %d)\n", lNum);
      exit(4);
    }
    if(strcmp(lArg2, "") != 0){
      printf("Error: .ORIG - unexpected extra operand (line %d)\n", lNum);
      exit(4);
    }
    int currAddr = toNum(lArg1);
    if (currAddr == -1){
      printf("Error: .ORIG - invalid operand (line %d)\n", lNum);
      exit(4);
    }
    if(currAddr < 0 || currAddr > 65535 || currAddr % 2 == 1){
      printf("Error: .ORIG - invalid constant (line %d)\n", lNum);
      exit(3);
    }
    fprintf(outfile, "0x%.4X\n", currAddr); 
    int orig = currAddr;
    *lLabel = '\0';
    *lOpcode = '\0';

//----------------------------------------- GENERATE SYMBOL TABLE ----------------------------------------------------------------------
    do
      {
        while(strcmp(lLabel, "") == 0 && strcmp(lOpcode, "") ==  0 && lRet != DONE){
          lRet = readAndParse(infile, lLine, &lLabel,
                  &lOpcode, &lArg1, &lArg2, &lArg3, &lArg4 );
          lNum++;
        }
        if(strcmp(lOpcode, ".end") == 0) break;

        if(currAddr > 65535){
          printf("Error: Not enough memory to store program (line %d)\n", lNum);
          exit(4);
        }   

        if( lRet != DONE && lRet != EMPTY_LINE )
        {
            if(strcmp(lLabel, "") != 0){
              
              //Check length
              int labLength = strlen(lLabel);
              if(labLength > 20){
                printf("Error: Label exceeds max length (line %d)\n", lNum);
                exit(4);
              }
              //Check alphanumeric
              for(int i = 0; i < labLength; i++){
                if(!isalnum(lLabel[i])){
                  printf("Error: Label must be alphanumeric (line %d)\n", lNum);
                  exit(4);
                }
              }
              //Check first character
              if(isdigit(lLabel[0]) || ((int) (lLabel[0]) == 120)){
                printf("Error: Label cannot begin with a number or 'x' (line %d)\n", lNum);
                exit(4);
              }
              //Check if label is register
              for(int i = 0; i < 8; i++){
                if(strcmp(lLabel, registers[i]) == 0){
                  printf("Error: Label cannot be a register (line %d)\n", lNum);
                  exit(4);
                }
              }
              //Check for unknown opcode (non-opcode on its own line)
              if(strcmp(lOpcode, "") == 0){
                printf("Error: Unknown opcode (line %d)\n", lNum);
                exit(2);
              }
              //Add the symbol
              if(addSymbol(lLabel, currAddr) == -1){
                printf("Error: Duplicate label (line %d)\n", lNum);
                exit(4);
              }
            }
        }
      currAddr += 2;

      //Get next line
      lRet = readAndParse(infile, lLine, &lLabel, &lOpcode, &lArg1, &lArg2, &lArg3, &lArg4 ); 
      lNum++;
    } while( lRet != DONE );

    if(strcmp(lOpcode, ".end") != 0){
      printf("Error: no .END found\n");
      exit(4);
    }
    if(strcmp(lLabel, "") != 0){
      printf("Error: Cannot label .END (line %d)\n", lNum);
      exit(4);
    }

    printf("First pass complete!\n\n");
    for(int i = 0; i < symTabLength; i++){
      printf("0x%.4x - %s\n", symbolTable[i].address, symbolTable[i].label);
    }

//----------------------------------------------- CONVERT TO BINARY ------------------------------------------------------------------
    rewind(infile);
    lNum = 1;
    currAddr = orig;
    lRet = OK;
    *lLabel = '\0';
    *lOpcode = '\0';

    //skip past .ORIG
    while(strcmp(lLabel, "") == 0 && strcmp(lOpcode, "") ==  0){
      lRet = readAndParse(infile, lLine, &lLabel, &lOpcode, &lArg1, &lArg2, &lArg3, &lArg4 );
      lNum++;
    }
    lRet = readAndParse(infile, lLine, &lLabel, &lOpcode, &lArg1, &lArg2, &lArg3, &lArg4 );
    
    //Start converting
    do{
      //Skip whitespace and comment-only lines
      while(strcmp(lLabel, "") == 0 && strcmp(lOpcode, "") ==  0){
          lRet = readAndParse(infile, lLine, &lLabel, &lOpcode, &lArg1, &lArg2, &lArg3, &lArg4 );
          lNum++;
      } 
      if(strcmp(lOpcode, ".end") == 0) break;

      //Perform conversion
      currAddr += 2;      //Pre-increment PC for offset calculations
      fprintf(outfile, "0x%.4X\n", convert(lOpcode, lArg1, lArg2, lArg3, lArg4, lNum, currAddr));

      //Get next line
      lRet = readAndParse(infile, lLine, &lLabel, &lOpcode, &lArg1, &lArg2, &lArg3, &lArg4 );
      lNum++;
    }
    while(lRet != DONE);

    printf("done\n");


    //Close files
    fclose(infile);
    fclose(outfile);
}

int toNum(char * pStr) {
    char * t_ptr;
    char * orig_pStr;
    int t_length,k;
    int lNum, lNeg = 0;
    long int lNumLong;

    orig_pStr = pStr;
    if( *pStr == '#' )                                /* decimal */
    {
      pStr++;
      if( *pStr == '-' )                                /* dec is negative */
      {
        lNeg = 1;
        pStr++;
      }
      t_ptr = pStr;
      t_length = strlen(t_ptr);
      for(k=0;k < t_length;k++)
      {
        if (!isdigit(*t_ptr))
        {
          printf("Error: invalid decimal operand, %s\n",orig_pStr);
          exit(4);
        }
        t_ptr++;
      }
      lNum = atoi(pStr);
      if (lNeg)
        lNum = -lNum;

      return lNum;
    }
    else if( *pStr == 'x' )        /* hex     */
    {
      pStr++;
      if( *pStr == '-' )                                /* hex is negative */
      {
        lNeg = 1;
        pStr++;
      }
      t_ptr = pStr;
      t_length = strlen(t_ptr);
      for(k=0;k < t_length;k++)
      {
        if (!isxdigit(*t_ptr))
        {
          printf("Error: invalid hex operand, %s\n",orig_pStr);
          exit(4);
        }
        t_ptr++;
      }
      lNumLong = strtol(pStr, NULL, 16);    /* convert hex string into integer */
      lNum = (lNumLong > INT_MAX)? INT_MAX : lNumLong;
      if( lNeg )
        lNum = -lNum;
      return lNum;
    }
    else
    {
          printf( "Error: invalid operand, %s\n", orig_pStr);
          exit(4);  /* This has been changed from error code 3 to error code 4, see clarification 12 */
    }
}

int readAndParse( FILE * pInfile, char * pLine, char ** pLabel, char ** pOpcode, 
    char ** pArg1, char ** pArg2, char ** pArg3, char ** pArg4) {
    char * lRet, * lPtr;
    int i;
    if(!fgets( pLine, MAX_LINE_LENGTH, pInfile ) )
        return( DONE );
    //Skip comment-only lines
    for( i = 0; i < strlen( pLine ); i++ )
        pLine[i] = tolower( pLine[i] );
    
    //Convert the line to lowercase
    *pLabel = *pOpcode = *pArg1 = *pArg2 = *pArg3 = *pArg4 = pLine + strlen(pLine);

    //Find a semicolon or end of line
    lPtr = pLine;

    while( *lPtr != ';' && *lPtr != '\0' && *lPtr != '\n' )
        lPtr++;

    *lPtr = '\0';
    if( !(lPtr = strtok( pLine, "\t\n ," ) ) )
        return( EMPTY_LINE );

    if( isOpcode( lPtr ) == -1 && lPtr[0] != '.' ) //Found a label if not opcode or pseudo-op
    {
        *pLabel = lPtr;
        if( !( lPtr = strtok( NULL, "\t\n ," ) ) ) return( OK );
    }
    
    *pOpcode = lPtr;

    if( !( lPtr = strtok( NULL, "\t\n ," ) ) ) return( OK );
    
    *pArg1 = lPtr;
    
    if( !( lPtr = strtok( NULL, "\t\n ," ) ) ) return( OK );

    *pArg2 = lPtr;
    if( !( lPtr = strtok( NULL, "\t\n ," ) ) ) return( OK );

    *pArg3 = lPtr;

    if( !( lPtr = strtok( NULL, "\t\n ," ) ) ) return( OK );

    *pArg4 = lPtr;

    return( OK );
}

int isOpcode(char * opcode) { 
    int match = -1;
    for(int i = 0; i < NUM_OPCODES; i++){
      if(strcmp(opcode, opcodes[i]) == 0){
        match = 0;
      }
    }
    return match;
}

int addSymbol(char * label, int addr){
  if(symTabLength >= 255) exit(4);

  //Check if duplicate label
  for(int i = 0; i < symTabLength; i++){
    if(strcmp(label, symbolTable[i].label) == 0){
      return -1;
    }
  }

  //Add symbol
  symbolTable[symTabLength].address = addr;
  strcpy(symbolTable[symTabLength].label, label);
  symTabLength++;

  return 0;
}

int convert(char * opcode, char * arg1, char * arg2, char * arg3, char * arg4, int lNum, int currAddr){
    //Determine opcode
    int opcIndex = -1;
    for(int i = 0; i < NUM_OPCODES; i++){
      if(strcmp(opcode, opcodes[i]) == 0){
        opcIndex = i;
        break;
      }
    }

    int bin = 0;
    //Big switch
    switch (opcIndex)
    {
//__________________________________________________ A D D | A N D | N O T | X O R _____________________________________________________________________________________
    case 0:
    case 1:
    case 18:
    case 26:
      if(opcIndex == 0) bin = 0x1 << 12; //opcode = 0001 if ADD
      if(opcIndex == 1) bin = 0x5 << 12; //opcode = 0101 if AND
      if(opcIndex == 18 || opcIndex == 26) bin = 0x9 << 12; //opcode = 1001 if XOR or NOT
      //Check number of operands
      if(strcmp(arg4, "") != 0 || (opcIndex == 18 && strcmp(arg3, "") != 0)) {
        printf("Error: unexpected extra operand (line %d)\n", lNum);
        exit(4);
      }
      //Add DR and SR1
      int dr = isReg(arg1);
      int sr1 = isReg(arg2);
      if(dr != -1 && sr1 != -1){
        bin += (dr << 9) + (sr1 << 6);
      }
      else{
        printf("Error: invalid operand (line %d)\n", lNum);
        exit(4);
      }
      //Add A and 3rd operand
      int sr2 = isReg(arg3);
      if(sr2 != -1){  //op3 is a register
        bin += sr2;  
      }
      else{           //op3 is an immediate or invalid
        int imm = -1;
        if(opcIndex != 18) imm = toNum(arg3); //Will exit(4) if arg3 isn't decimal or hex operand
        if(imm < -16 || imm > 15){
          printf("Error: imm5 too large (line %d)\n", lNum);
          exit(3);
        }
        bin += (1 << 5) + (imm5(imm)); //[5] = 1 for immediate
      }
      break;
//________________________________________________________ B R _________________________________________________________________________________________________
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      if(opcIndex == 2) opcIndex = 9; //Treat BR as BRnzp
      //Check number and type of operands
      if(strcmp(arg2, "") != 0 || strcmp (arg1, "") == 0){
        printf("Error: unexpected extra operand (line %d)\n", lNum);
        exit(4);
      }
      //Calculate offset
      int labAddr = isLabel(arg1, lNum);
      if(labAddr != -1){
        int offset = (labAddr - currAddr) >> 1;  //PCoffset9 represents number of instructions (addresses x2)
        if(offset < -256 || offset > 255){
          printf("Error: off9 too large (line %d)\n", lNum);
          exit(3);
        }
        bin += (opcIndex - 2) << 9; //set bits [11:9]
        bin += off9(offset);
      }
      else{
        printf("Error: invalid label (line %d)\n", lNum);
        exit(1);
      }
      break;    
//______________________________________________ J M P | R E T _______________________________________________________________________
    case 10:
    case 11:
      if(strcmp(arg2, "") != 0 || (opcIndex == 11 && strcmp(arg1, "") != 0)) {
          printf("Error: unexpected extra operand (line %d)\n", lNum);
          exit(4);
      }
      int base = isReg(arg1);
      if(opcIndex == 11){
        base = 7;
        bin = 0xC1C0;
        break;
      }
      if(base == -1){
        printf("Error: invalid operand (line %d)\n", lNum);
        exit(4);
      }
      bin += (0xC << 12) + (base << 6); //opcode = 1100, [8:6] signify register
      break;
//________________________________________________ J S R ______________________________________________________________________
    case 12:
      bin = 0x48 << 8;  //opcode = 0100, [11] = 1
      if(strcmp(arg2, "") != 0 || strcmp(arg1, "") == 0){
        printf("Error: unexpected extra operand (line %d)\n", lNum);
        exit(4);
      }
      //Calculate offset
      labAddr = isLabel(arg1, lNum);
      if(labAddr != -1){
        int offset = (labAddr - currAddr) >> 1;  //PCoffset11 represents number of instructions (addresses x2)
        if(offset < -1024 || offset > 1023){
          printf("Error: off11 too large (line %d)\n", lNum);
          exit(4);
        }
        bin += off11(offset);
      }
      else{
        printf("Error: invalid label (line %d)\n", lNum);
        exit(1);
      }
      break;

//____________________________________________ J S R R __________________________________________________________________________
    case 13: 
      bin = 0x4 << 12; //opcode = 0100
      if(strcmp(arg2, "") != 0){
        printf("Error: unexpected extra operand (line %d)\n", lNum);
        exit(4);
      }
      base = isReg(arg1);
      if(base == -1){
        printf("Error: invalid operand (line %d)\n", lNum);
        exit(4);
      }
      bin += base << 6;
      break;
// _____________________________________ L D B | L D W | S T B | S T W __________________________________________________________
    case 14:
    case 15:
    case 24:
    case 23:
      if(strcmp(arg4, "") != 0) {
        printf("Error: unexpected extra operand (line %d)\n", lNum);
        exit(4);
      }
      if(opcIndex == 14) bin = 0x2 << 12;
      else if(opcIndex == 15) bin = 0x6 << 12;
      else if(opcIndex == 24) bin = 0x3 << 12;
      else bin = 0x7 << 12;
      //Add DR and SR1
      dr = isReg(arg1);
      sr1 = isReg(arg2);
      if(dr != -1 && sr1 != -1){
        bin += (dr << 9) + (sr1 << 6);
      }
      else{
        printf("Error: invalid operand (line %d)\n", lNum);
        exit(4);
      }
      //Handle offset
      int offset = toNum(arg3);  //Will exit(4) if arg3 isn't decimal or hex operand
      if(offset < -32 || offset > 31){
          printf("Error: off6 too large (line %d)\n", lNum);
          exit(3);
      }
      bin += off6(offset);
      break;
//_______________________________________________ L E A _______________________________________________________________
    case 16:
      bin = 0xE << 12; //opcode = 1110 for LEA
      if(strcmp(arg3, "") != 0){
        printf("Error: unexpected extra operand (line %d)\n", lNum);
        exit(4);
      }
      //Handle DR
      dr = isReg(arg1);
      if(dr == -1){
        printf("Error: invalid operand (line %d)\n", lNum);
        exit(4);
      }
      bin += dr << 9; //[11:9] = DR
      //Calculate offset
      labAddr = isLabel(arg2, lNum);
      if(labAddr != -1){
        int offset = (labAddr - currAddr) >> 1;  //PCoffset9 represents number of instructions (addresses x2)
        if(offset < -256 || offset > 255){
          printf("Error: off9 too large (line %d)\n", lNum);
          exit(4);
        }
        bin += off9(offset);
      }
      else{
        printf("Error: invalid label (line %d)\n", lNum);
        exit(1);
      }
      break;
//________________________________________________ N O P | R T I ____________________________________________________________________
    case 17:
    case 19:
      if(strcmp(arg1, "") != 0){
        printf("Error: unexpected extra operand (line %d)\n", lNum);
        exit(4);
      }
      if(opcIndex == 19) bin = 0x8 << 12;
      break;
//________________________________________ L S H F | R S H F L | R S H F A __________________________________________________________
    case 20:
    case 22:
    case 21:
      if(strcmp(arg4, "") != 0) {
        printf("Error: unexpected extra operand (line %d)\n", lNum);
        exit(4);
      }
      bin = 0xD << 12; //opcode = 1101
      if(opcIndex == 22){
        bin += 1 << 4; //[5] = 1 for arithmetic shift
      }
      if(opcIndex == 21){
        bin += 3 << 4; //[4] = 1 for right shift
      }
      //Add DR and SR1
      dr = isReg(arg1);
      sr1 = isReg(arg2);
      if(dr != -1 && sr1 != -1){
        bin += (dr << 9) + (sr1 << 6);
      }
      else{
        printf("Error: invalid operand (line %d)\n", lNum);
        exit(4);
      }
      int amt = toNum(arg3); //Will exit(4) if arg3 isn't decimal or hex operand
      if(amt < 0 || amt > 15){
          printf("Error: amount4 too large (line %d)\n", lNum);
          exit(3);        
      }
      bin += amt;
      break;
//__________________________________________________ T R A P | H A L T ________________________________________________________
    case 25:
    case 27:
      if(strcmp(arg2, "") != 0 || (opcIndex == 27 && strcmp(arg1, "") != 0)) {
          printf("Error: unexpected extra operand (line %d)\n", lNum);
          exit(4);
      }
      bin = 0xF << 12; //opcode = 1111
      int vect = 0;
      if(opcIndex == 27){ 
        vect = 0x25;
        arg1[0] = 'x';
      } 
      else vect = toNum(arg1);
      if(arg1[0] != 'x'){
        printf("Error: trapvect8 must be a hex number\n");
        exit(4);
      }
      if(vect < 0 || vect > 255){
        printf("Error: trapvect8 too large (line %d)\n", lNum);
        exit(3);
      }
      bin += vect;
      break;
//__________________________________________________ . F I L L ________________________________________________________________
    case 32:
      if(strcmp(arg2, "") != 0){
        printf("Error: unexpected extra operand (line %d)\n", lNum);
        exit(4);
      }
      int num = toNum(arg1);
      if(num < -32768 || num > 65535){
        printf("Error: .fill argument too large (line %d)\n", lNum);
        exit(3);
      }
      bin = imm16(num);
      break;

//___________________________________________________ O T H E R ________________________________________________________________ 
    default:
      printf("Error: Unknown opcode (line %d)\n", lNum);
      exit(2);
      break;
    }

    return bin;
}

int isReg(char * lArg){
    for(int i = 0; i < 8; i++){
      if(strcmp(lArg, registers[i]) == 0){
        return i;
      }
    }
    return -1;
}

int isLabel(char * lArg, int lNum){
  if(isReg(lArg) != -1 || lArg[0] == 'x'){
    printf("Error: invalid operand (line %d)\n", lNum);
    exit(4);        
  }
  for(int i = 0; i < strlen(lArg); i++){
    if(!isalnum(lArg[i])) {
      printf("Error: invalid operand (line %d)\n", lNum);
      exit(4);                 
    }
  }
  for(int i = 0; i < symTabLength; i++){
    if(strcmp(lArg, symbolTable[i].label) == 0){
      return symbolTable[i].address;
    }
  }
  return -1;
}