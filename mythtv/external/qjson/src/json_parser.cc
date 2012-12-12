/* A Bison parser, made by GNU Bison 2.5.  */

/* Skeleton implementation for Bison LALR(1) parsers in C++
   
      Copyright (C) 2002-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* First part of user declarations.  */

/* Line 293 of lalr1.cc  */
#line 25 "json_parser.yy"

  #include "parser_p.h"
  #include "json_scanner.h"
  #include "qjson_debug.h"

  #include <QtCore/QByteArray>
  #include <QtCore/QMap>
  #include <QtCore/QString>
  #include <QtCore/QVariant>

  #include <limits>

  class JSonScanner;

  namespace QJson {
    class Parser;
  }

  #define YYERROR_VERBOSE 1


/* Line 293 of lalr1.cc  */
#line 61 "json_parser.tab.cc"


#include "json_parser.hh"

/* User implementation prologue.  */


/* Line 299 of lalr1.cc  */
#line 70 "json_parser.tab.cc"

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* FIXME: INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                               \
 do                                                                    \
   if (N)                                                              \
     {                                                                 \
       (Current).begin = YYRHSLOC (Rhs, 1).begin;                      \
       (Current).end   = YYRHSLOC (Rhs, N).end;                        \
     }                                                                 \
   else                                                                \
     {                                                                 \
       (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;        \
     }                                                                 \
 while (false)
#endif

/* Suppress unused-variable warnings by "using" E.  */
#define YYUSE(e) ((void) (e))

/* Enable debugging if requested.  */
#if YYDEBUG

/* A pseudo ostream that takes yydebug_ into account.  */
# define YYCDEBUG if (yydebug_) (*yycdebug_)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)	\
do {							\
  if (yydebug_)						\
    {							\
      *yycdebug_ << Title << ' ';			\
      yy_symbol_print_ ((Type), (Value), (Location));	\
      *yycdebug_ << std::endl;				\
    }							\
} while (false)

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug_)				\
    yy_reduce_print_ (Rule);		\
} while (false)

# define YY_STACK_PRINT()		\
do {					\
  if (yydebug_)				\
    yystack_print_ ();			\
} while (false)

#else /* !YYDEBUG */

# define YYCDEBUG if (false) std::cerr
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_REDUCE_PRINT(Rule)
# define YY_STACK_PRINT()

#endif /* !YYDEBUG */

#define yyerrok		(yyerrstatus_ = 0)
#define yyclearin	(yychar = yyempty_)

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab
#define YYRECOVERING()  (!!yyerrstatus_)


namespace yy {

/* Line 382 of lalr1.cc  */
#line 156 "json_parser.tab.cc"

  /* Return YYSTR after stripping away unnecessary quotes and
     backslashes, so that it's suitable for yyerror.  The heuristic is
     that double-quoting is unnecessary unless the string contains an
     apostrophe, a comma, or backslash (other than backslash-backslash).
     YYSTR is taken from yytname.  */
  std::string
  json_parser::yytnamerr_ (const char *yystr)
  {
    if (*yystr == '"')
      {
        std::string yyr = "";
        char const *yyp = yystr;

        for (;;)
          switch (*++yyp)
            {
            case '\'':
            case ',':
              goto do_not_strip_quotes;

            case '\\':
              if (*++yyp != '\\')
                goto do_not_strip_quotes;
              /* Fall through.  */
            default:
              yyr += *yyp;
              break;

            case '"':
              return yyr;
            }
      do_not_strip_quotes: ;
      }

    return yystr;
  }


  /// Build a parser object.
  json_parser::json_parser (QJson::ParserPrivate* driver_yyarg)
    :
#if YYDEBUG
      yydebug_ (false),
      yycdebug_ (&std::cerr),
#endif
      driver (driver_yyarg)
  {
  }

  json_parser::~json_parser ()
  {
  }

#if YYDEBUG
  /*--------------------------------.
  | Print this symbol on YYOUTPUT.  |
  `--------------------------------*/

  inline void
  json_parser::yy_symbol_value_print_ (int yytype,
			   const semantic_type* yyvaluep, const location_type* yylocationp)
  {
    YYUSE (yylocationp);
    YYUSE (yyvaluep);
    switch (yytype)
      {
         default:
	  break;
      }
  }


  void
  json_parser::yy_symbol_print_ (int yytype,
			   const semantic_type* yyvaluep, const location_type* yylocationp)
  {
    *yycdebug_ << (yytype < yyntokens_ ? "token" : "nterm")
	       << ' ' << yytname_[yytype] << " ("
	       << *yylocationp << ": ";
    yy_symbol_value_print_ (yytype, yyvaluep, yylocationp);
    *yycdebug_ << ')';
  }
#endif

  void
  json_parser::yydestruct_ (const char* yymsg,
			   int yytype, semantic_type* yyvaluep, location_type* yylocationp)
  {
    YYUSE (yylocationp);
    YYUSE (yymsg);
    YYUSE (yyvaluep);

    YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

    switch (yytype)
      {
  
	default:
	  break;
      }
  }

  void
  json_parser::yypop_ (unsigned int n)
  {
    yystate_stack_.pop (n);
    yysemantic_stack_.pop (n);
    yylocation_stack_.pop (n);
  }

#if YYDEBUG
  std::ostream&
  json_parser::debug_stream () const
  {
    return *yycdebug_;
  }

  void
  json_parser::set_debug_stream (std::ostream& o)
  {
    yycdebug_ = &o;
  }


  json_parser::debug_level_type
  json_parser::debug_level () const
  {
    return yydebug_;
  }

  void
  json_parser::set_debug_level (debug_level_type l)
  {
    yydebug_ = l;
  }
#endif

  inline bool
  json_parser::yy_pact_value_is_default_ (int yyvalue)
  {
    return yyvalue == yypact_ninf_;
  }

  inline bool
  json_parser::yy_table_value_is_error_ (int yyvalue)
  {
    return yyvalue == yytable_ninf_;
  }

  int
  json_parser::parse ()
  {
    /// Lookahead and lookahead in internal form.
    int yychar = yyempty_;
    int yytoken = 0;

    /* State.  */
    int yyn;
    int yylen = 0;
    int yystate = 0;

    /* Error handling.  */
    int yynerrs_ = 0;
    int yyerrstatus_ = 0;

    /// Semantic value of the lookahead.
    semantic_type yylval;
    /// Location of the lookahead.
    location_type yylloc;
    /// The locations where the error started and ended.
    location_type yyerror_range[3];

    /// $$.
    semantic_type yyval;
    /// @$.
    location_type yyloc;

    int yyresult;

    YYCDEBUG << "Starting parse" << std::endl;


    /* Initialize the stacks.  The initial state will be pushed in
       yynewstate, since the latter expects the semantical and the
       location values to have been already stored, initialize these
       stacks with a primary value.  */
    yystate_stack_ = state_stack_type (0);
    yysemantic_stack_ = semantic_stack_type (0);
    yylocation_stack_ = location_stack_type (0);
    yysemantic_stack_.push (yylval);
    yylocation_stack_.push (yylloc);

    /* New state.  */
  yynewstate:
    yystate_stack_.push (yystate);
    YYCDEBUG << "Entering state " << yystate << std::endl;

    /* Accept?  */
    if (yystate == yyfinal_)
      goto yyacceptlab;

    goto yybackup;

    /* Backup.  */
  yybackup:

    /* Try to take a decision without lookahead.  */
    yyn = yypact_[yystate];
    if (yy_pact_value_is_default_ (yyn))
      goto yydefault;

    /* Read a lookahead token.  */
    if (yychar == yyempty_)
      {
	YYCDEBUG << "Reading a token: ";
	yychar = yylex (&yylval, &yylloc, driver);
      }


    /* Convert token to internal form.  */
    if (yychar <= yyeof_)
      {
	yychar = yytoken = yyeof_;
	YYCDEBUG << "Now at end of input." << std::endl;
      }
    else
      {
	yytoken = yytranslate_ (yychar);
	YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
      }

    /* If the proper action on seeing token YYTOKEN is to reduce or to
       detect an error, take that action.  */
    yyn += yytoken;
    if (yyn < 0 || yylast_ < yyn || yycheck_[yyn] != yytoken)
      goto yydefault;

    /* Reduce or error.  */
    yyn = yytable_[yyn];
    if (yyn <= 0)
      {
	if (yy_table_value_is_error_ (yyn))
	  goto yyerrlab;
	yyn = -yyn;
	goto yyreduce;
      }

    /* Shift the lookahead token.  */
    YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

    /* Discard the token being shifted.  */
    yychar = yyempty_;

    yysemantic_stack_.push (yylval);
    yylocation_stack_.push (yylloc);

    /* Count tokens shifted since error; after three, turn off error
       status.  */
    if (yyerrstatus_)
      --yyerrstatus_;

    yystate = yyn;
    goto yynewstate;

  /*-----------------------------------------------------------.
  | yydefault -- do the default action for the current state.  |
  `-----------------------------------------------------------*/
  yydefault:
    yyn = yydefact_[yystate];
    if (yyn == 0)
      goto yyerrlab;
    goto yyreduce;

  /*-----------------------------.
  | yyreduce -- Do a reduction.  |
  `-----------------------------*/
  yyreduce:
    yylen = yyr2_[yyn];
    /* If YYLEN is nonzero, implement the default value of the action:
       `$$ = $1'.  Otherwise, use the top of the stack.

       Otherwise, the following line sets YYVAL to garbage.
       This behavior is undocumented and Bison
       users should not rely upon it.  */
    if (yylen)
      yyval = yysemantic_stack_[yylen - 1];
    else
      yyval = yysemantic_stack_[0];

    {
      slice<location_type, location_stack_type> slice (yylocation_stack_, yylen);
      YYLLOC_DEFAULT (yyloc, slice, yylen);
    }
    YY_REDUCE_PRINT (yyn);
    switch (yyn)
      {
	  case 2:

/* Line 690 of lalr1.cc  */
#line 84 "json_parser.yy"
    {
              driver->m_result = (yysemantic_stack_[(1) - (1)]);
              qjsonDebug() << "json_parser - parsing finished";
            }
    break;

  case 3:

/* Line 690 of lalr1.cc  */
#line 89 "json_parser.yy"
    { (yyval) = (yysemantic_stack_[(1) - (1)]); }
    break;

  case 4:

/* Line 690 of lalr1.cc  */
#line 91 "json_parser.yy"
    {
            qCritical()<< "json_parser - syntax error found, "
                    << "forcing abort, Line" << (yyloc).begin.line << "Column" << (yyloc).begin.column;
            YYABORT;
          }
    break;

  case 6:

/* Line 690 of lalr1.cc  */
#line 98 "json_parser.yy"
    { (yyval) = (yysemantic_stack_[(3) - (2)]); }
    break;

  case 7:

/* Line 690 of lalr1.cc  */
#line 100 "json_parser.yy"
    { (yyval) = QVariant (QVariantMap()); }
    break;

  case 8:

/* Line 690 of lalr1.cc  */
#line 101 "json_parser.yy"
    {
            QVariantMap members = (yysemantic_stack_[(2) - (2)]).toMap();
            (yysemantic_stack_[(2) - (2)]) = QVariant(); // Allow reuse of map
            (yyval) = QVariant(members.unite ((yysemantic_stack_[(2) - (1)]).toMap()));
          }
    break;

  case 9:

/* Line 690 of lalr1.cc  */
#line 107 "json_parser.yy"
    { (yyval) = QVariant (QVariantMap()); }
    break;

  case 10:

/* Line 690 of lalr1.cc  */
#line 108 "json_parser.yy"
    {
          QVariantMap members = (yysemantic_stack_[(3) - (3)]).toMap();
          (yysemantic_stack_[(3) - (3)]) = QVariant(); // Allow reuse of map
          (yyval) = QVariant(members.unite ((yysemantic_stack_[(3) - (2)]).toMap()));
          }
    break;

  case 11:

/* Line 690 of lalr1.cc  */
#line 114 "json_parser.yy"
    {
            QVariantMap pair;
            pair.insert ((yysemantic_stack_[(3) - (1)]).toString(), QVariant((yysemantic_stack_[(3) - (3)])));
            (yyval) = QVariant (pair);
          }
    break;

  case 12:

/* Line 690 of lalr1.cc  */
#line 120 "json_parser.yy"
    { (yyval) = (yysemantic_stack_[(3) - (2)]); }
    break;

  case 13:

/* Line 690 of lalr1.cc  */
#line 122 "json_parser.yy"
    { (yyval) = QVariant (QVariantList()); }
    break;

  case 14:

/* Line 690 of lalr1.cc  */
#line 123 "json_parser.yy"
    {
          QVariantList members = (yysemantic_stack_[(2) - (2)]).toList();
          (yysemantic_stack_[(2) - (2)]) = QVariant(); // Allow reuse of list
          members.prepend ((yysemantic_stack_[(2) - (1)]));
          (yyval) = QVariant(members);
        }
    break;

  case 15:

/* Line 690 of lalr1.cc  */
#line 130 "json_parser.yy"
    { (yyval) = QVariant (QVariantList()); }
    break;

  case 16:

/* Line 690 of lalr1.cc  */
#line 131 "json_parser.yy"
    {
            QVariantList members = (yysemantic_stack_[(3) - (3)]).toList();
            (yysemantic_stack_[(3) - (3)]) = QVariant(); // Allow reuse of list
            members.prepend ((yysemantic_stack_[(3) - (2)]));
            (yyval) = QVariant(members);
          }
    break;

  case 17:

/* Line 690 of lalr1.cc  */
#line 138 "json_parser.yy"
    { (yyval) = (yysemantic_stack_[(1) - (1)]); }
    break;

  case 18:

/* Line 690 of lalr1.cc  */
#line 139 "json_parser.yy"
    { (yyval) = (yysemantic_stack_[(1) - (1)]); }
    break;

  case 19:

/* Line 690 of lalr1.cc  */
#line 140 "json_parser.yy"
    { (yyval) = (yysemantic_stack_[(1) - (1)]); }
    break;

  case 20:

/* Line 690 of lalr1.cc  */
#line 141 "json_parser.yy"
    { (yyval) = (yysemantic_stack_[(1) - (1)]); }
    break;

  case 21:

/* Line 690 of lalr1.cc  */
#line 142 "json_parser.yy"
    { (yyval) = QVariant (true); }
    break;

  case 22:

/* Line 690 of lalr1.cc  */
#line 143 "json_parser.yy"
    { (yyval) = QVariant (false); }
    break;

  case 23:

/* Line 690 of lalr1.cc  */
#line 144 "json_parser.yy"
    {
          QVariant null_variant;
          (yyval) = null_variant;
        }
    break;

  case 24:

/* Line 690 of lalr1.cc  */
#line 149 "json_parser.yy"
    { (yyval) = QVariant(QVariant::Double); (yyval).setValue( -std::numeric_limits<double>::infinity() ); }
    break;

  case 25:

/* Line 690 of lalr1.cc  */
#line 150 "json_parser.yy"
    { (yyval) = QVariant(QVariant::Double); (yyval).setValue( std::numeric_limits<double>::infinity() ); }
    break;

  case 26:

/* Line 690 of lalr1.cc  */
#line 151 "json_parser.yy"
    { (yyval) = QVariant(QVariant::Double); (yyval).setValue( std::numeric_limits<double>::quiet_NaN() ); }
    break;

  case 28:

/* Line 690 of lalr1.cc  */
#line 154 "json_parser.yy"
    {
            if ((yysemantic_stack_[(1) - (1)]).toByteArray().startsWith('-')) {
              (yyval) = QVariant (QVariant::LongLong);
              (yyval).setValue((yysemantic_stack_[(1) - (1)]).toLongLong());
            }
            else {
              (yyval) = QVariant (QVariant::ULongLong);
              (yyval).setValue((yysemantic_stack_[(1) - (1)]).toULongLong());
            }
          }
    break;

  case 29:

/* Line 690 of lalr1.cc  */
#line 164 "json_parser.yy"
    {
            const QByteArray value = (yysemantic_stack_[(2) - (1)]).toByteArray() + (yysemantic_stack_[(2) - (2)]).toByteArray();
            (yyval) = QVariant(QVariant::Double);
            (yyval).setValue(value.toDouble());
          }
    break;

  case 30:

/* Line 690 of lalr1.cc  */
#line 169 "json_parser.yy"
    { (yyval) = QVariant ((yysemantic_stack_[(2) - (1)]).toByteArray() + (yysemantic_stack_[(2) - (2)]).toByteArray()); }
    break;

  case 31:

/* Line 690 of lalr1.cc  */
#line 170 "json_parser.yy"
    {
            const QByteArray value = (yysemantic_stack_[(3) - (1)]).toByteArray() + (yysemantic_stack_[(3) - (2)]).toByteArray() + (yysemantic_stack_[(3) - (3)]).toByteArray();
            (yyval) = QVariant (value);
          }
    break;

  case 32:

/* Line 690 of lalr1.cc  */
#line 175 "json_parser.yy"
    { (yyval) = QVariant ((yysemantic_stack_[(2) - (1)]).toByteArray() + (yysemantic_stack_[(2) - (2)]).toByteArray()); }
    break;

  case 33:

/* Line 690 of lalr1.cc  */
#line 176 "json_parser.yy"
    { (yyval) = QVariant (QByteArray("-") + (yysemantic_stack_[(3) - (2)]).toByteArray() + (yysemantic_stack_[(3) - (3)]).toByteArray()); }
    break;

  case 34:

/* Line 690 of lalr1.cc  */
#line 178 "json_parser.yy"
    { (yyval) = QVariant (QByteArray("")); }
    break;

  case 35:

/* Line 690 of lalr1.cc  */
#line 179 "json_parser.yy"
    {
          (yyval) = QVariant((yysemantic_stack_[(2) - (1)]).toByteArray() + (yysemantic_stack_[(2) - (2)]).toByteArray());
        }
    break;

  case 36:

/* Line 690 of lalr1.cc  */
#line 183 "json_parser.yy"
    {
          (yyval) = QVariant(QByteArray(".") + (yysemantic_stack_[(2) - (2)]).toByteArray());
        }
    break;

  case 37:

/* Line 690 of lalr1.cc  */
#line 187 "json_parser.yy"
    { (yyval) = QVariant((yysemantic_stack_[(2) - (1)]).toByteArray() + (yysemantic_stack_[(2) - (2)]).toByteArray()); }
    break;

  case 38:

/* Line 690 of lalr1.cc  */
#line 189 "json_parser.yy"
    { (yyval) = (yysemantic_stack_[(3) - (2)]); }
    break;

  case 39:

/* Line 690 of lalr1.cc  */
#line 191 "json_parser.yy"
    { (yyval) = QVariant (QString(QLatin1String(""))); }
    break;

  case 40:

/* Line 690 of lalr1.cc  */
#line 192 "json_parser.yy"
    {
                (yyval) = (yysemantic_stack_[(1) - (1)]);
              }
    break;



/* Line 690 of lalr1.cc  */
#line 770 "json_parser.tab.cc"
	default:
          break;
      }
    /* User semantic actions sometimes alter yychar, and that requires
       that yytoken be updated with the new translation.  We take the
       approach of translating immediately before every use of yytoken.
       One alternative is translating here after every semantic action,
       but that translation would be missed if the semantic action
       invokes YYABORT, YYACCEPT, or YYERROR immediately after altering
       yychar.  In the case of YYABORT or YYACCEPT, an incorrect
       destructor might then be invoked immediately.  In the case of
       YYERROR, subsequent parser actions might lead to an incorrect
       destructor call or verbose syntax error message before the
       lookahead is translated.  */
    YY_SYMBOL_PRINT ("-> $$ =", yyr1_[yyn], &yyval, &yyloc);

    yypop_ (yylen);
    yylen = 0;
    YY_STACK_PRINT ();

    yysemantic_stack_.push (yyval);
    yylocation_stack_.push (yyloc);

    /* Shift the result of the reduction.  */
    yyn = yyr1_[yyn];
    yystate = yypgoto_[yyn - yyntokens_] + yystate_stack_[0];
    if (0 <= yystate && yystate <= yylast_
	&& yycheck_[yystate] == yystate_stack_[0])
      yystate = yytable_[yystate];
    else
      yystate = yydefgoto_[yyn - yyntokens_];
    goto yynewstate;

  /*------------------------------------.
  | yyerrlab -- here on detecting error |
  `------------------------------------*/
  yyerrlab:
    /* Make sure we have latest lookahead translation.  See comments at
       user semantic actions for why this is necessary.  */
    yytoken = yytranslate_ (yychar);

    /* If not already recovering from an error, report this error.  */
    if (!yyerrstatus_)
      {
	++yynerrs_;
	if (yychar == yyempty_)
	  yytoken = yyempty_;
	error (yylloc, yysyntax_error_ (yystate, yytoken));
      }

    yyerror_range[1] = yylloc;
    if (yyerrstatus_ == 3)
      {
	/* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

	if (yychar <= yyeof_)
	  {
	  /* Return failure if at end of input.  */
	  if (yychar == yyeof_)
	    YYABORT;
	  }
	else
	  {
	    yydestruct_ ("Error: discarding", yytoken, &yylval, &yylloc);
	    yychar = yyempty_;
	  }
      }

    /* Else will try to reuse lookahead token after shifting the error
       token.  */
    goto yyerrlab1;


  /*---------------------------------------------------.
  | yyerrorlab -- error raised explicitly by YYERROR.  |
  `---------------------------------------------------*/
  yyerrorlab:

    /* Pacify compilers like GCC when the user code never invokes
       YYERROR and the label yyerrorlab therefore never appears in user
       code.  */
    if (false)
      goto yyerrorlab;

    yyerror_range[1] = yylocation_stack_[yylen - 1];
    /* Do not reclaim the symbols of the rule which action triggered
       this YYERROR.  */
    yypop_ (yylen);
    yylen = 0;
    yystate = yystate_stack_[0];
    goto yyerrlab1;

  /*-------------------------------------------------------------.
  | yyerrlab1 -- common code for both syntax error and YYERROR.  |
  `-------------------------------------------------------------*/
  yyerrlab1:
    yyerrstatus_ = 3;	/* Each real token shifted decrements this.  */

    for (;;)
      {
	yyn = yypact_[yystate];
	if (!yy_pact_value_is_default_ (yyn))
	{
	  yyn += yyterror_;
	  if (0 <= yyn && yyn <= yylast_ && yycheck_[yyn] == yyterror_)
	    {
	      yyn = yytable_[yyn];
	      if (0 < yyn)
		break;
	    }
	}

	/* Pop the current state because it cannot handle the error token.  */
	if (yystate_stack_.height () == 1)
	YYABORT;

	yyerror_range[1] = yylocation_stack_[0];
	yydestruct_ ("Error: popping",
		     yystos_[yystate],
		     &yysemantic_stack_[0], &yylocation_stack_[0]);
	yypop_ ();
	yystate = yystate_stack_[0];
	YY_STACK_PRINT ();
      }

    yyerror_range[2] = yylloc;
    // Using YYLLOC is tempting, but would change the location of
    // the lookahead.  YYLOC is available though.
    YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
    yysemantic_stack_.push (yylval);
    yylocation_stack_.push (yyloc);

    /* Shift the error token.  */
    YY_SYMBOL_PRINT ("Shifting", yystos_[yyn],
		     &yysemantic_stack_[0], &yylocation_stack_[0]);

    yystate = yyn;
    goto yynewstate;

    /* Accept.  */
  yyacceptlab:
    yyresult = 0;
    goto yyreturn;

    /* Abort.  */
  yyabortlab:
    yyresult = 1;
    goto yyreturn;

  yyreturn:
    if (yychar != yyempty_)
      {
        /* Make sure we have latest lookahead translation.  See comments
           at user semantic actions for why this is necessary.  */
        yytoken = yytranslate_ (yychar);
        yydestruct_ ("Cleanup: discarding lookahead", yytoken, &yylval,
                     &yylloc);
      }

    /* Do not reclaim the symbols of the rule which action triggered
       this YYABORT or YYACCEPT.  */
    yypop_ (yylen);
    while (yystate_stack_.height () != 1)
      {
	yydestruct_ ("Cleanup: popping",
		   yystos_[yystate_stack_[0]],
		   &yysemantic_stack_[0],
		   &yylocation_stack_[0]);
	yypop_ ();
      }

    return yyresult;
  }

  // Generate an error message.
  std::string
  json_parser::yysyntax_error_ (int yystate, int yytoken)
  {
    std::string yyres;
    // Number of reported tokens (one for the "unexpected", one per
    // "expected").
    size_t yycount = 0;
    // Its maximum.
    enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
    // Arguments of yyformat.
    char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];

    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yytoken) is
         if this state is a consistent state with a default action.
         Thus, detecting the absence of a lookahead is sufficient to
         determine that there is no unexpected or expected token to
         report.  In that case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is
         a consistent state with a default action.  There might have
         been a previous inconsistent state, consistent state with a
         non-default action, or user semantic action that manipulated
         yychar.
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state
         merging (from LALR or IELR) and default reductions corrupt the
         expected token list.  However, the list is correct for
         canonical LR with one exception: it will still contain any
         token that will not be accepted due to an error action in a
         later state.
    */
    if (yytoken != yyempty_)
      {
        yyarg[yycount++] = yytname_[yytoken];
        int yyn = yypact_[yystate];
        if (!yy_pact_value_is_default_ (yyn))
          {
            /* Start YYX at -YYN if negative to avoid negative indexes in
               YYCHECK.  In other words, skip the first -YYN actions for
               this state because they are default actions.  */
            int yyxbegin = yyn < 0 ? -yyn : 0;
            /* Stay within bounds of both yycheck and yytname.  */
            int yychecklim = yylast_ - yyn + 1;
            int yyxend = yychecklim < yyntokens_ ? yychecklim : yyntokens_;
            for (int yyx = yyxbegin; yyx < yyxend; ++yyx)
              if (yycheck_[yyx + yyn] == yyx && yyx != yyterror_
                  && !yy_table_value_is_error_ (yytable_[yyx + yyn]))
                {
                  if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                    {
                      yycount = 1;
                      break;
                    }
                  else
                    yyarg[yycount++] = yytname_[yyx];
                }
          }
      }

    char const* yyformat = 0;
    switch (yycount)
      {
#define YYCASE_(N, S)                         \
        case N:                               \
          yyformat = S;                       \
        break
        YYCASE_(0, YY_("syntax error"));
        YYCASE_(1, YY_("syntax error, unexpected %s"));
        YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
        YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
        YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
        YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
      }

    // Argument number.
    size_t yyi = 0;
    for (char const* yyp = yyformat; *yyp; ++yyp)
      if (yyp[0] == '%' && yyp[1] == 's' && yyi < yycount)
        {
          yyres += yytnamerr_ (yyarg[yyi++]);
          ++yyp;
        }
      else
        yyres += *yyp;
    return yyres;
  }


  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
  const signed char json_parser::yypact_ninf_ = -21;
  const signed char
  json_parser::yypact_[] =
  {
         3,   -21,   -21,    -6,    31,   -10,     0,   -21,   -21,   -21,
       6,   -21,   -21,    25,   -21,   -21,   -21,   -21,   -21,   -21,
      -5,   -21,    22,    19,    21,    23,    24,     0,   -21,     0,
     -21,   -21,    13,   -21,     0,     0,    29,   -21,   -21,    -6,
     -21,    31,   -21,    31,   -21,   -21,   -21,   -21,   -21,   -21,
     -21,    19,   -21,    24,   -21,   -21
  };

  /* YYDEFACT[S] -- default reduction number in state S.  Performed when
     YYTABLE doesn't specify something else to do.  Zero means the
     default is an error.  */
  const unsigned char
  json_parser::yydefact_[] =
  {
         0,     5,     4,     7,    13,     0,    34,    21,    22,    23,
      39,    25,    26,     0,     2,    19,    20,     3,    18,    27,
      28,    17,     0,     9,     0,     0,    15,    34,    24,    34,
      32,    40,     0,     1,    34,    34,    29,    30,     6,     0,
       8,     0,    12,     0,    14,    33,    35,    38,    36,    37,
      31,     9,    11,    15,    10,    16
  };

  /* YYPGOTO[NTERM-NUM].  */
  const signed char
  json_parser::yypgoto_[] =
  {
       -21,   -21,   -21,   -21,   -21,   -20,     4,   -21,   -21,   -18,
      -4,   -21,   -21,   -21,   -14,   -21,    -3,    -1,   -21
  };

  /* YYDEFGOTO[NTERM-NUM].  */
  const signed char
  json_parser::yydefgoto_[] =
  {
        -1,    13,    14,    15,    22,    40,    23,    16,    25,    44,
      17,    18,    19,    20,    30,    36,    37,    21,    32
  };

  /* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule which
     number is the opposite.  If YYTABLE_NINF_, syntax error.  */
  const signed char json_parser::yytable_ninf_ = -1;
  const unsigned char
  json_parser::yytable_[] =
  {
        26,    27,    24,     1,     2,    34,     3,    35,     4,    28,
      10,    29,     5,    45,     6,    46,     7,     8,     9,    10,
      48,    49,    11,    12,    31,    33,    38,    39,    41,    42,
      47,    54,    43,    50,     3,    55,     4,    52,    24,    53,
       5,    35,     6,    51,     7,     8,     9,    10,     0,     0,
      11,    12
  };

  /* YYCHECK.  */
  const signed char
  json_parser::yycheck_[] =
  {
         4,    11,     3,     0,     1,    10,     3,    12,     5,    19,
      16,    11,     9,    27,    11,    29,    13,    14,    15,    16,
      34,    35,    19,    20,    18,     0,     4,     8,     7,     6,
      17,    51,     8,    36,     3,    53,     5,    41,    39,    43,
       9,    12,    11,    39,    13,    14,    15,    16,    -1,    -1,
      19,    20
  };

  /* STOS_[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
  const unsigned char
  json_parser::yystos_[] =
  {
         0,     0,     1,     3,     5,     9,    11,    13,    14,    15,
      16,    19,    20,    22,    23,    24,    28,    31,    32,    33,
      34,    38,    25,    27,    38,    29,    31,    11,    19,    11,
      35,    18,    39,     0,    10,    12,    36,    37,     4,     8,
      26,     7,     6,     8,    30,    35,    35,    17,    35,    35,
      37,    27,    31,    31,    26,    30
  };

#if YYDEBUG
  /* TOKEN_NUMBER_[YYLEX-NUM] -- Internal symbol number corresponding
     to YYLEX-NUM.  */
  const unsigned short int
  json_parser::yytoken_number_[] =
  {
         0,   256,   257,     1,     2,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18
  };
#endif

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
  const unsigned char
  json_parser::yyr1_[] =
  {
         0,    21,    22,    23,    23,    23,    24,    25,    25,    26,
      26,    27,    28,    29,    29,    30,    30,    31,    31,    31,
      31,    31,    31,    31,    32,    32,    32,    32,    33,    33,
      33,    33,    34,    34,    35,    35,    36,    37,    38,    39,
      39
  };

  /* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
  const unsigned char
  json_parser::yyr2_[] =
  {
         0,     2,     1,     1,     1,     1,     3,     0,     2,     0,
       3,     3,     3,     0,     2,     0,     3,     1,     1,     1,
       1,     1,     1,     1,     2,     1,     1,     1,     1,     2,
       2,     3,     2,     3,     0,     2,     2,     2,     3,     0,
       1
  };

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
  /* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
     First, the terminals, then, starting at \a yyntokens_, nonterminals.  */
  const char*
  const json_parser::yytname_[] =
  {
    "\"end of file\"", "error", "$undefined", "\"{\"", "\"}\"", "\"[\"",
  "\"]\"", "\":\"", "\",\"", "\"-\"", "\".\"", "\"digit\"",
  "\"exponential\"", "\"true\"", "\"false\"", "\"null\"",
  "\"open quotation mark\"", "\"close quotation mark\"", "\"string\"",
  "\"Infinity\"", "\"NaN\"", "$accept", "start", "data", "object",
  "members", "r_members", "pair", "array", "values", "r_values", "value",
  "special_or_number", "number", "int", "digits", "fract", "exp", "string",
  "string_arg", 0
  };
#endif

#if YYDEBUG
  /* YYRHS -- A `-1'-separated list of the rules' RHS.  */
  const json_parser::rhs_number_type
  json_parser::yyrhs_[] =
  {
        22,     0,    -1,    23,    -1,    31,    -1,     1,    -1,     0,
      -1,     3,    25,     4,    -1,    -1,    27,    26,    -1,    -1,
       8,    27,    26,    -1,    38,     7,    31,    -1,     5,    29,
       6,    -1,    -1,    31,    30,    -1,    -1,     8,    31,    30,
      -1,    38,    -1,    32,    -1,    24,    -1,    28,    -1,    13,
      -1,    14,    -1,    15,    -1,     9,    19,    -1,    19,    -1,
      20,    -1,    33,    -1,    34,    -1,    34,    36,    -1,    34,
      37,    -1,    34,    36,    37,    -1,    11,    35,    -1,     9,
      11,    35,    -1,    -1,    11,    35,    -1,    10,    35,    -1,
      12,    35,    -1,    16,    39,    17,    -1,    -1,    18,    -1
  };

  /* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
     YYRHS.  */
  const unsigned char
  json_parser::yyprhs_[] =
  {
         0,     0,     3,     5,     7,     9,    11,    15,    16,    19,
      20,    24,    28,    32,    33,    36,    37,    41,    43,    45,
      47,    49,    51,    53,    55,    58,    60,    62,    64,    66,
      69,    72,    76,    79,    83,    84,    87,    90,    93,    97,
      98
  };

  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
  const unsigned char
  json_parser::yyrline_[] =
  {
         0,    84,    84,    89,    90,    96,    98,   100,   101,   107,
     108,   114,   120,   122,   123,   130,   131,   138,   139,   140,
     141,   142,   143,   144,   149,   150,   151,   152,   154,   164,
     169,   170,   175,   176,   178,   179,   183,   187,   189,   191,
     192
  };

  // Print the state stack on the debug stream.
  void
  json_parser::yystack_print_ ()
  {
    *yycdebug_ << "Stack now";
    for (state_stack_type::const_iterator i = yystate_stack_.begin ();
	 i != yystate_stack_.end (); ++i)
      *yycdebug_ << ' ' << *i;
    *yycdebug_ << std::endl;
  }

  // Report on the debug stream that the rule \a yyrule is going to be reduced.
  void
  json_parser::yy_reduce_print_ (int yyrule)
  {
    unsigned int yylno = yyrline_[yyrule];
    int yynrhs = yyr2_[yyrule];
    /* Print the symbols being reduced, and their result.  */
    *yycdebug_ << "Reducing stack by rule " << yyrule - 1
	       << " (line " << yylno << "):" << std::endl;
    /* The symbols being reduced.  */
    for (int yyi = 0; yyi < yynrhs; yyi++)
      YY_SYMBOL_PRINT ("   $" << yyi + 1 << " =",
		       yyrhs_[yyprhs_[yyrule] + yyi],
		       &(yysemantic_stack_[(yynrhs) - (yyi + 1)]),
		       &(yylocation_stack_[(yynrhs) - (yyi + 1)]));
  }
#endif // YYDEBUG

  /* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
  json_parser::token_number_type
  json_parser::yytranslate_ (int t)
  {
    static
    const token_number_type
    translate_table[] =
    {
           0,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2
    };
    if ((unsigned int) t <= yyuser_token_number_max_)
      return translate_table[t];
    else
      return yyundef_token_;
  }

  const int json_parser::yyeof_ = 0;
  const int json_parser::yylast_ = 51;
  const int json_parser::yynnts_ = 19;
  const int json_parser::yyempty_ = -2;
  const int json_parser::yyfinal_ = 33;
  const int json_parser::yyterror_ = 1;
  const int json_parser::yyerrcode_ = 256;
  const int json_parser::yyntokens_ = 21;

  const unsigned int json_parser::yyuser_token_number_max_ = 257;
  const json_parser::token_number_type json_parser::yyundef_token_ = 2;


} // yy

/* Line 1136 of lalr1.cc  */
#line 1303 "json_parser.tab.cc"


/* Line 1138 of lalr1.cc  */
#line 196 "json_parser.yy"


int yy::yylex(YYSTYPE *yylval, yy::location *yylloc, QJson::ParserPrivate* driver)
{
  JSonScanner* scanner = driver->m_scanner;
  yylval->clear();
  int ret = scanner->yylex(yylval, yylloc);

  qjsonDebug() << "json_parser::yylex - calling scanner yylval==|"
           << yylval->toByteArray() << "|, ret==|" << QString::number(ret) << "|";
  
  return ret;
}

void yy::json_parser::error (const yy::location& yyloc,
                                 const std::string& error)
{
  /*qjsonDebug() << yyloc.begin.line;
  qjsonDebug() << yyloc.begin.column;
  qjsonDebug() << yyloc.end.line;
  qjsonDebug() << yyloc.end.column;*/
  qjsonDebug() << "json_parser::error [line" << yyloc.end.line << "] -" << error.c_str() ;
  driver->setError(QString::fromLatin1(error.c_str()), yyloc.end.line);
}

