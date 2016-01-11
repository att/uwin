.H 1 "\f5iffe\fP  IF Feature Exists?"
.P
\f5iffe\fP is a tool to generate a header file that
answers one or more questions about the current compiler
or system that is being compile on.
\f5iffe\fP process files that are normally stored in a
directory named \f5features\fP.  The resulting header
files are stored in a directory named \f5FEATURE\fP.
Programs that use \f5iffe\fP include these files
and use macros from these files for conditional
compilation tests.
.P
\f5iffe\fP is designed to be able to test for any
type of behavior of the target system.
However, since the following types of questions
can be specified very simply with \f5iffe\fP:
.BL
.LI
Does header file \fIname\fP\f5.h\fP exist as a standard header?
.nf
\f5hdr	\fP\fIname\fP
.fi
.LI
Does header file \fIsys/name\fP\f5.h\fP exist as a standard header?
.nf
\f5sys	\fP\fIname\fP
.fi
.LI
Does the type \fItype\fP exist when a given set of standard include
files are included?
.nf
\f5typ	\fP\fIname\fP\f5	\fP\fIfiles\fP
.fi
.LI
Is the function \fIname\fP in the standard library of one of
the give libraries?
.nf
\f5lib	\fP\fIname\fP
.fi
.LI
Is the data symbol \fIname\fP in the standard library of one of
the give libraries?
.nf
\f5dat	\fP\fIname\fP
.fi
.LI
Is the symbol \fIname\fP  a member of a given structure when
a give set of include files is given?
.nf
\f5mem	\fP\fIstruct_name.member_name\fP\f5        \fP\fIfiles\fP
.fi
.LI
Is the command \fIname\fP found on the standard path?
.nf
\f5cmd	\fP\fIname\fP
.fi
.LE
Wherever, a \fIname\fP can be specified, a comma separated
list of names can be specified instead and the test will
be performed for each \fIname\fP in the list.
.P
In addition, named tests can be specified by \f5iffe\fP. 
Comments in an \f5iffe\fP. file are specified with \f5note{\fP...\f5}\fP.
