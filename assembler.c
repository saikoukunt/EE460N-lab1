#include <stdio.h> /* standard input/output library */
#include <stdlib.h> /* Standard C Library */
#include <string.h> /* String operations library */
#include <ctype.h> /* Library for useful character operations */
#include <limits.h> /* Library for definitions of common variable type characteristics */

//D E F I N I T I O N S
#define NUM_OPCODES 32
#define MAX_LINE_LENGTH 255
#define MAX_LABEL_LEN 20
#define MAX_SYMBOLS 255

typedef struct {
	int address;
	char label[MAX_LABEL_LEN + 1];	//Add 1 for null termination
} TableEntry;

enum {
    DONE, OK, EMPTY_LINE
};

char * opcodes[33] = {"add", "and", "br", "brn", "brz", "brp", "brnz", "brnp", "brzp",
"brnzp", "jmp", "ret", "jsr", "jsrr", "ldb", "ldw", "lea", "nop", "not", "ret", "rti", "lshf", "rshfl", 
"rshfa", "stb", "stw", "trap", "xor", "halt", "in", "out", "getc", "puts" 
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

    printf("program name = '%s'\n", prgName);
    printf("input file name = '%s'\n", iFileName);
    printf("output file name = '%s'\n", oFileName);

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

    //.ORIG handling
    if(strcmp(lOpcode, ".orig") != 0){
      printf("Error: file does not start with .ORIG\n");
      exit(4);
    }
    if(strcmp(lArg2, "") != 0){
      printf("Error: .ORIG - unexpected extra operand (line %d)\n", lNum);
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

    //Generate symbol table
    do
      {
        while(strcmp(lLabel, "") == 0 && strcmp(lOpcode, "") ==  0){
          lRet = readAndParse(infile, lLine, &lLabel,
                  &lOpcode, &lArg1, &lArg2, &lArg3, &lArg4 );
          lNum++;
        }
        if(currAddr > 65535){
          printf("Error: Not enough memory to store program");
          exit(4);
        }
        lRet = readAndParse(infile, lLine, &lLabel,
                &lOpcode, &lArg1, &lArg2, &lArg3, &lArg4 );    
        lNum++;    

        if( lRet != DONE && lRet != EMPTY_LINE )
        {
            if(strcmp(lLabel, "") != 0){
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
              
              if(addSymbol(lLabel, currAddr) == -1){
                printf("Error: Duplicate label (line %d)\n", lNum);
                exit(4);
              }
            }
        }
        currAddr += 2;
      } while( lRet != DONE );

    for(int i = 0; i < symTabLength; i++){
      printf("0x%.4x - %s\n", symbolTable[i].address, symbolTable[i].label);
    }

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
    if( !fgets( pLine, MAX_LINE_LENGTH, pInfile ) )
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
  if(symTabLength > 255) exit(4);

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