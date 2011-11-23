


%{
/*
 * PARSER.Y --grammar definition
 *
 * We use the flex generated lexical analyser to get the input tokens
 *
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <memory.h>
 #include <string.h>
 #include <ctype.h>
 #include <time.h>

 #include "gcode_parser.h"

 #ifndef TRUE
 #define TRUE 1
 #endif

 #ifndef FALSE
 #define FALSE 0
 #endif

  int yylex(void);

 extern FILE *yyin;

typedef int Symbol_t;

void yyerror(char *s);

extern struct ParseState_t *pParseState;

extern void StatementType(const int type);
extern void AddToHeeks();
extern void ResetForEndOfBlock();

double Value(const int name);

const char *StringFromDouble( const double name );
const char *StringDuplication( const char *value );

int AddSymbol( const char * name, const double value );
int Symbol( const char * name );

int LHSassignmentfromRHS( const int lhs, const int rhs );

int LHSequivalenttoRHS(const int lhs, const int rhs);
int LHSnotequaltoRHS(const int lhs, const int rhs);
int LHSgreaterthanRHS(const int lhs, const int rhs);
int LHSlessthanRHS(const int lhs, const int rhs);

int LHSplusRHS(const int lhs, const int rhs);
int LHSminusRHS(const int lhs, const int rhs);
int LHStimesRHS(const int lhs, const int rhs);
int LHSdividedbyRHS(const int lhs, const int rhs);

void SwitchParseUnits( const int metric );

int ASin(const int symbol_id);
int ACos(const int symbol_id);
int ATan(const int symbol_id);

int Sin(const int symbol_id);
int Cos(const int symbol_id);
int Tan(const int symbol_id);

int AbsoluteValue(const int symbol_id);
int	Sqrt(const int symbol_id);

%}

%union {			/* lexer return types... */
	const char	*string;	/* string value of most other things */
	int		integer;
	double	floating_point_number;
	int		symbol;
	int		boolean;
}




%token <string>			COMMENTS
%token <integer>		END_BLOCK
%token <string>			LINE_NUMBER

%token <integer>		EQ
%token <integer>		NE
%token <integer>		LT
%token <integer>		GT

%token <integer>		AND
%token <integer>		OR

%token <integer>		IF
%token <integer>		ELSE
%token <integer>		ENDIF
%token <string>			O_CODE

%token <integer>		HASH
%token <string>			NAME

%token <integer>		SIN
%token <integer>		COS
%token <integer>		TAN

%token <integer>		ASIN
%token <integer>		ACOS
%token <integer>		ATAN

%token <integer>		ABS
%token <integer>		SQRT

%token <integer>		G00
%token <integer>		G01
%token <integer>		G02
%token <integer>		G03
%token <integer>		G04
%token <integer>		G10
%token <integer>		G17
%token <integer>		G18
%token <integer>		G19
%token <integer>		G20
%token <integer>		G21
%token <integer>		G28
%token <integer>		G28_1
%token <integer>		G30
%token <integer>		G30_1
%token <integer>		G33
%token <integer>		G33_1
%token <integer>		G38_2
%token <integer>		G38_3
%token <integer>		G38_4
%token <integer>		G38_5
%token <integer>		G40
%token <integer>		G41
%token <integer>		G42
%token <integer>		G43
%token <integer>		G43_1
%token <integer>		G49
%token <integer>		G53
%token <integer>		G54
%token <integer>		G55
%token <integer>		G56
%token <integer>		G57
%token <integer>		G58
%token <integer>		G59
%token <integer>		G59_1
%token <integer>		G59_2
%token <integer>		G59_3
%token <integer>		G61
%token <integer>		G61_1
%token <integer>		G64
%token <integer>		G80
%token <integer>		G81
%token <integer>		G82
%token <integer>		G83
%token <integer>		G84
%token <integer>		G85
%token <integer>		G89
%token <integer>		G90
%token <integer>		G91
%token <integer>		G92
%token <integer>		G92_1
%token <integer>		G92_2
%token <integer>		G92_3
%token <integer>		G93
%token <integer>		G94
%token <integer>		G95
%token <integer>		G98
%token <integer>		G99

%token <integer>		M00
%token <integer>		M01
%token <integer>		M02
%token <integer>		M03
%token <integer>		M04
%token <integer>		M05
%token <integer>		M06
%token <integer>		M07
%token <integer>		M08
%token <integer>		M09
%token <integer>		M60
%token <integer>		USER_DEFINED_M_CODES

%token <integer>		X
%token <integer>		Y
%token <integer>		Z

%token <integer>		A
%token <integer>		B
%token <integer>		C

%token <integer>		U
%token <integer>		V
%token <integer>		W

%token <integer>		I
%token <integer>		J
%token <integer>		K

%token <integer>		L
%token <integer>		P
%token <integer>		Q
%token <integer>		R

%token <integer>		TOOL_SELECTION		// Tn
%token <integer>		TOOL_NUMBER			// Dn for within the G41 or G42 codes.

%token <integer>		SPINDLE_SPEED

%token <floating_point_number>		NUMBER_TOKEN
%token <integer>		OPEN_SQUARE_BRACKET
%token <integer>		CLOSE_SQUARE_BRACKET
%token <integer>		PLUS
%token <integer>		MINUS
%token <integer>		MULTIPLY
%token <integer>		DIVIDE
%token <integer>		ASSIGNMENT
%token <string>			VARIABLE
%token <foating_point_number>		FEEDRATE

%type <symbol>		MathematicalExpression
%type <floating_point_number>		Arguments
%type <floating_point_number>		Argument
%type <boolean>						Comparison
%type <boolean>						BooleanExpression
%type <symbol>			Variable
%type <symbol>			Functions

%%


ExpressionList:			ExpressionList Expression {	}
	|					Expression { }
	;

Expression:				COMMENTS
						{
							strncpy( pParseState->comment, $1, sizeof(pParseState->comment)-1 );
							StatementType( stComment );
						}

	|					LINE_NUMBER { strncpy( pParseState->line_number, $1, sizeof(pParseState->line_number)-1 ); }
	|					Variable ASSIGNMENT MathematicalExpression { LHSassignmentfromRHS( $1, $3); StatementType(stVariable); }
	|					FEEDRATE MathematicalExpression { pParseState->feed_rate = Value($2); StatementType( stPreparation ); }
	|					SPINDLE_SPEED MathematicalExpression { pParseState->spindle_speed = $2; StatementType( stPreparation ); }
	|					TOOL_SELECTION { pParseState->tool_slot_number = $1; StatementType( stToolChange ); }
	|					MCodes { StatementType( stProgram ); }
	|					GCodes { StatementType( stProgram ); }
	|					ProgramFlow { StatementType( stProgram ); }
	;

MCodes:					M00 { /* pause program */ }
	|					M01 { }
	|					M03 { /* turn spindle clockwise */ }
	|					M04 { /* turn spingle counter-clockwise */ }
	|					M05 { /* spindle stop */ }
	|					M06 { /* enable selected tool */ StatementType( stToolChange ); }
	|					M07 { /* mist coolant on */ }
	|					M08 { /* flood coolant on */ }
	|					M09 { /* coolant off */ }
	|					M60 { }
	|					M02
							{
								// End of program.
								AddToHeeks();
								ResetForEndOfBlock();
								return(0);
							}
	|					USER_DEFINED_M_CODES { }
	;

GCodes:					Motion { }
	|					NonModalCodes { StatementType( stPreparation ); }
	|					DistanceMode { StatementType( stPreparation ); }
	|					FeedRateMode { StatementType( stPreparation ); }
	|					ToolLengthOffset { StatementType( stPreparation ); }
	|					Units { StatementType( stPreparation ); }
	|					PlaneSelection { StatementType( stAxis ); }
	|					CutterRadiusCompensation { StatementType( stPreparation ); }
	|					PathControlMode { StatementType( stPreparation ); }
	|					ReturnModeInCannedCycles { StatementType( stPreparation ); }
	|					CoordinateSystems  { StatementType( stPreparation ); }
	;

CoordinateSystems:		G53 { pParseState->current_coordinate_system = csG53; }
	|					G54 { pParseState->modal_coordinate_system = csG54; }
	|					G55 { pParseState->modal_coordinate_system = csG55; }
	|					G56 { pParseState->modal_coordinate_system = csG56; }
	|					G57 { pParseState->modal_coordinate_system = csG57; }
	|					G58 { pParseState->modal_coordinate_system = csG58; }
	|					G59 { pParseState->modal_coordinate_system = csG59; }
	|					G59_1 { pParseState->modal_coordinate_system = csG59_1; }
	|					G59_2 { pParseState->modal_coordinate_system = csG59_2; }
	|					G59_3 { pParseState->modal_coordinate_system = csG59_3; }
	;

ReturnModeInCannedCycles:	G98 { }
	|					G99 { }
	;

PathControlMode:		G61 { }
	|					G61_1 { }
	|					G64 Arguments  { }
	;

CutterRadiusCompensation:	G40 { }
	|					G41 TOOL_NUMBER { }
	|					G42 TOOL_NUMBER { }
	;

PlaneSelection:			G17 { pParseState->plane = eXYPlane; }
	|					G18 { pParseState->plane = eXZPlane; }
	|					G19 { pParseState->plane = eYZPlane; }
	;

Units:					G20 { SwitchParseUnits( 0 ); }
	|					G21 { SwitchParseUnits( 1 ); }
	;

DistanceMode:			G90 { StatementType( stPreparation ); }
	|					G91 { StatementType( stPreparation ); }
	;

FeedRateMode:			G93 { StatementType( stPreparation ); }
	|					G94 { StatementType( stPreparation ); }
	|					G95 { StatementType( stPreparation ); }
	;

ToolLengthOffset:		G43 Arguments { /* turn on cutter length compensation */ StatementType( stToolLengthEnabled ); }
	|					G43_1 Arguments{ /* turn on cutter length compensation */ StatementType( stToolLengthEnabled ); }
	|					G49 { /* turn off cutter length compensation */ StatementType( stToolLengthDisabled ); }
	;

Motion:					G00   Arguments	{ pParseState->statement_type = stRapid; }
	|					G01   Arguments	{ pParseState->statement_type = stFeed; }
	|					G02   Arguments	{ pParseState->statement_type = stArcClockwise; }
	|					G03   Arguments	{ pParseState->statement_type = stArcCounterClockwise; }
	|					G04   Arguments { pParseState->statement_type = stFeed; }
	|					G33   Arguments { pParseState->statement_type = stFeed; }
	|					G33_1 Arguments { pParseState->statement_type = stFeed; }
	|					G38_2 Arguments { pParseState->statement_type = stProbe; }
	|					G38_3 Arguments { pParseState->statement_type = stProbe; }
	|					G38_4 Arguments { pParseState->statement_type = stProbe; }
	|					G38_5 Arguments { pParseState->statement_type = stProbe; }
	|					G80   { }
	|					G81   Arguments { pParseState->statement_type = stDrilling; }
	|					G82   Arguments { pParseState->statement_type = stDrilling; }
	|					G83   Arguments { pParseState->statement_type = stDrilling; }
	|					G84   Arguments { pParseState->statement_type = stTapping; }
	|					G85   Arguments { pParseState->statement_type = stDrilling; }
	|					G89   Arguments { }
	;

NonModalCodes:
	|					G28 { StatementType( stG28 ); }
	|					G28_1 { /* save current position */ }
	|					G30   { StatementType( stG30 ); }
	|					G30_1 { /* save current position */ }
	|					G92 Arguments { StatementType( stG92 ); }
	|					G92_1 { StatementType( stG92_1 ); }
	|					G92_2 { StatementType( stG92_2 ); }
	|					G92_3 { StatementType( stG92_3 ); }
	|					G10 Arguments { StatementType( stDataSetting ); }
	;


ProgramFlow:			O_CODE { }
	|					IF BooleanExpression { }
	|					ELSE { }
	|					ENDIF { }
	|					END_BLOCK
						{
							AddToHeeks();
							ResetForEndOfBlock();
							pParseState->line_offset += 1;
							if (pParseState->current_coordinate_system == csG53)
							{
								pParseState->current_coordinate_system = pParseState->modal_coordinate_system;
							}
						}
	;

// Define a list of G10_argument (singular) values.
Arguments:			Arguments Argument {}
	|					Argument { }
	;

Argument:				L MathematicalExpression { Value($2); pParseState->l = Value($2); pParseState->l_specified = 1; }
	|					P MathematicalExpression { Value($2); pParseState->p = Value($2); pParseState->p_specified = 1; }
	|					R MathematicalExpression { Value($2); pParseState->r = Value($2); pParseState->r_specified = 1; }
	|					Q MathematicalExpression { Value($2); pParseState->q = Value($2); pParseState->q_specified = 1; }
	|					X MathematicalExpression { Value($2); pParseState->x = Value($2); pParseState->x_specified = 1; }
	|					Y MathematicalExpression { Value($2); pParseState->y = Value($2); pParseState->y_specified = 1; }
	|					Z MathematicalExpression { Value($2); pParseState->z = Value($2); pParseState->z_specified = 1; }
	|					A MathematicalExpression { Value($2); pParseState->a = Value($2); pParseState->a_specified = 1; }
	|					B MathematicalExpression { Value($2); pParseState->b = Value($2); pParseState->b_specified = 1; }
	|					C MathematicalExpression { Value($2); pParseState->c = Value($2); pParseState->c_specified = 1; }
	|					U MathematicalExpression { Value($2); pParseState->u = Value($2); pParseState->u_specified = 1; }
	|					V MathematicalExpression { Value($2); pParseState->v = Value($2); pParseState->v_specified = 1; }
	|					W MathematicalExpression { Value($2); pParseState->w = Value($2); pParseState->w_specified = 1; }
	|					I MathematicalExpression { Value($2); pParseState->i = Value($2); pParseState->i_specified = 1; }
	|					J MathematicalExpression { Value($2); pParseState->j = Value($2); pParseState->j_specified = 1; }
	|					K MathematicalExpression { Value($2); pParseState->k = Value($2); pParseState->k_specified = 1; }
	;
	;


MathematicalExpression:		OPEN_SQUARE_BRACKET MathematicalExpression CLOSE_SQUARE_BRACKET { $$ = $2; }
	|						MathematicalExpression PLUS MathematicalExpression { $$ = LHSplusRHS($1, $3); }
	|						MathematicalExpression MINUS MathematicalExpression { $$ = LHSminusRHS($1, $3); }
	|						MathematicalExpression MULTIPLY MathematicalExpression { $$ = LHStimesRHS($1, $3); }
	|						MathematicalExpression DIVIDE MathematicalExpression { $$ = LHSdividedbyRHS($1, $3); }
	|						NUMBER_TOKEN { $$ = AddSymbol("", $1); }
	|						Variable { $$ = $1; }
	|						Functions { $$ = $1; }
	;

Functions:					ASIN MathematicalExpression { $$ = ASin($2); }
	|						ACOS MathematicalExpression { $$ = ACos($2); }
	|						ATAN MathematicalExpression { $$ = ATan($2); }
	|						SIN MathematicalExpression { $$ = Sin($2); }
	|						COS MathematicalExpression { $$ = Cos($2); }
	|						TAN MathematicalExpression { $$ = Tan($2); }
	|						ABS MathematicalExpression { $$ = AbsoluteValue($2); }
	|						SQRT MathematicalExpression { $$ = Sqrt($2); }
	;

Comparison:					OPEN_SQUARE_BRACKET Comparison CLOSE_SQUARE_BRACKET { $$ = $2; }
	|						MathematicalExpression EQ MathematicalExpression { $$ = LHSequivalenttoRHS($1, $3); }
	|						MathematicalExpression NE MathematicalExpression { $$ = LHSnotequaltoRHS($1, $3); }
	|						MathematicalExpression GT MathematicalExpression { $$ = LHSgreaterthanRHS($1, $3); }
	|						MathematicalExpression LT MathematicalExpression { $$ = LHSlessthanRHS($1, $3); }
	;

BooleanExpression:			Comparison AND Comparison { $$ = (int) (($1 != 0) && ($3 != 0))?1:0; }
	|						Comparison OR Comparison { $$ = (int) (($1 != 0) || ($3 != 0))?1:0; }
	|						Comparison { $$ = $1; }
	|						BooleanExpression AND BooleanExpression { $$ = (int) (($1 != 0) && ($3 != 0))?1:0; }
	|						BooleanExpression OR BooleanExpression { $$ = (int) (($1 != 0) || ($3 != 0))?1:0; }
	|						OPEN_SQUARE_BRACKET BooleanExpression CLOSE_SQUARE_BRACKET { $$ = $2; }
	;

Variable:					HASH NUMBER_TOKEN { $$ = Symbol( StringFromDouble($2)); }
	|						HASH NAME { $$ = Symbol($2); }
	;

%%



