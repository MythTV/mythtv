% lthpurisa.fd
%
% This file is part of the thaifonts-scalable package
%
% Copyright (C) 1999 National Electronics and Computer Technology Center
% (NECTEC), Thailand.  All rights reserved.
% 
% It may be distributed and/or modified under the
% conditions of the LaTeX Project Public License, either version 1.3
% of this license or (at your option) any later version.
% The latest version of this license is in
%   http://www.latex-project.org/lppl.txt
% and version 1.3 or later is part of all distributions of LaTeX
% version 2005/12/01 or later.
% 
% This work has the LPPL maintenance status "maintained".
% 
% The Current Maintainer of this work is Theppitak Karoonboonyanan.
% 
% 2004/02/05 Poonlap Veerathanabutr <poonlap@linux.thai.net>
%            - first release (based on lthgaruda.fd)

\ProvidesFile{lthpurisa.fd}[2004/02/05 v1.0 Thai font definitions]

% Primary declarations
\DeclareFontFamily{LTH}{purisa}{}
\DeclareFontShape{LTH}{purisa}{m}{n}{<->s * purisa}{}
\DeclareFontShape{LTH}{purisa}{m}{sl}{<->s * purisa_o}{}
%%%%%%% bold series
\DeclareFontShape{LTH}{purisa}{b}{n}{<->s * purisa_b}{}
\DeclareFontShape{LTH}{purisa}{b}{sl}{<->s * purisa_bo}{}

% Substitutions
\DeclareFontShape{LTH}{purisa}{m}{it}{<->ssub * purisa/m/sl}{}
\DeclareFontShape{LTH}{purisa}{b}{it}{<->ssub * purisa/b/sl}{}

\DeclareFontShape{LTH}{purisa}{bx}{n}{<->ssub * purisa/b/n}{}
\DeclareFontShape{LTH}{purisa}{bx}{sl}{<->ssub * purisa/b/sl}{}
\DeclareFontShape{LTH}{purisa}{bx}{it}{<->ssub * purisa/b/sl}{}
\endinput

% 
% EOF
%
