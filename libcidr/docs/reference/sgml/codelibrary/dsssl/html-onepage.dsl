<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY lt			CDATA	"&#60;"		-- less-than sign -->
<!ENTITY hash		CDATA	"&#35;"		-- less-than sign -->
<!ENTITY loccss		CDATA	"codelibrary-html.css">
] >

;; HTML-specific stylesheet.
;; <!-- $Id: html-onepage.dsl,v 1.10 2003/07/15 19:06:41 fullermd Exp $ -->

;; Pull in the ability
(declare-flow-object-class formatting-instruction
	"UNREGISTERED::James Clark//Flow Object Class::formatting-instruction")

;; Some useful defs
(define (newline)
	(make formatting-instruction
		data: "&#13;"))

(define (mktag contents)
	(make paragraph
		(make formatting-instruction
			data: "&lt;")
		(make formatting-instruction
			data: contents)
		(make formatting-instruction
			data: ">")
		(newline)))

(define (optag)
	(make formatting-instruction
		data: "&lt;"))

(define (closetag)
	(make formatting-instruction
		data: ">"))


;; Now some more us-specific stuff
(define (print-head-section)
	(make paragraph
		(mktag "head")
		(mktag "title")
		(literal (attribute-string "name"))
		(literal " - Code Documentation")
		(newline)
		(mktag "/title")
		(mktag "meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\"")
		(mktag "meta name=\"Generator\" content=\"Codelibrary DTD DSSSL\"")
		(mktag "style type=\"text/css\" media=\"all\"")
		(literal "@import \"&loccss;\";")
		(mktag "/style")
		(mktag "/head")
	)
)


;; Outside-in
(element codelibrary
	(make simple-page-sequence
		(mktag "!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"
	\"http://www.w3.org/TR/html4/strict.dtd\"")
		(mktag "html lang=\"en\"")
		(print-head-section)
		(mktag "body")
		(process-matching-children 'geninfo)
		(print-contents-list)
		(process-matching-children 'datastructs)
		(process-matching-children 'functions)
		(newline)
		(mktag "/body")
		(mktag "/html")))

;; geninfo section
(element geninfo
	(make display-group
		(mktag "div class=\"geninfo\"")
		(mktag "h1 id=\"geninfo\"")
		(literal "Summary")
		(newline)
		(mktag "/h1")
		(process-children)
		(newline)
		(mktag "/div")))

;; Datastructs
(element datastructs
	(make display-group
		(mktag "div class=\"datastructs\"")
		(if (first-sibling?)
			(make paragraph
				(mktag "h1 id=\"datastructs\"")
				(literal "Data structures")
				(newline)
				(mktag "/h1"))
			(empty-sosofo))
		(mktag "ul")
		(print-struct-pub)
		(print-struct-priv)
		(newline)
		(mktag "/ul")
		(mktag "/div")))

;;(element (datastructs datatype)
;; In pub/priv modes

;; Maybe we should use a DL instead of a UL
(element (datatype name)
	(make sequence
		(mktag "span class=\"structdef\"")
		(process-children)
		(literal ": ")))

(element (datatype desc)
	(make sequence
		(process-children)
		(newline)
		(mktag "/span")))

(element (datatype note)
	(make paragraph
		(mktag "li")
		(mktag "div class=\"note\"")
		(mktag "p")
		(literal "Note:")
		(newline)
		(mktag "/p")
		(process-children)
		(newline)
		(mktag "/div")))

;; Note that this covers (datatype member) and (member member)
(element member
	(make display-group
		(if (first-sibling?)
			(make paragraph
				(if (not (have-ancestor? "MEMBER"))
					(make paragraph
						(mktag "li")
						(literal "Members:")
						(newline))
					(empty-sosofo))
				(mktag "ul")
				(mktag "li"))
			(make paragraph
				(mktag "li")))
		(make paragraph
			(process-children))
		(if (last-sibling?)
			(make paragraph
				(mktag "/ul"))
			(empty-sosofo))))

(element (member type)
	(make sequence
		(mktag "span class=\"memberdef\"")
		(process-children)
		(literal " ")))

(element (member name)
	(make sequence
		(process-children)
		(literal ":")
		(newline)
		(mktag "/span")))

;; Now, let's break into functions
(element functions
	(make display-group
		(mktag "div class=\"functions\"")
		(if (first-sibling?)
			(make paragraph
				(mktag "h1 id=\"functions\"")
				(literal "Functions")
				(newline)
				(mktag "/h1"))
			(empty-sosofo))
		(mktag "ul")
		(print-func-pub)
		(print-func-priv)
		(newline)
		(mktag "/ul")
		(mktag "/div")))

;;(element (functions function)
;; This is done in the -pub and -priv modes

(element (function name)
	(make sequence
		(mktag "span class=\"funcdef\"")
		(process-children)
		(literal "(): ")))

(element (function desc)
	(make sequence
		(process-children)
		(newline)
		(mktag "/span")))

(element (function longdesc)
	(make display-group
		(mktag "li")
		(literal "Summary:")
		(newline)
		(process-children)
		(newline)))

(element (function args)
	(make display-group
		(make paragraph
			(mktag "li")
			(literal "Arguments:")
			(newline)
			(mktag "ul"))
		(if (= (node-list-length (children (current-node))) 0)
			(make paragraph
				(mktag "li")
				(literal "None")
				(newline)
				(mktag "/li"))
			(process-children))
		(make paragraph
			(mktag "/ul"))))

(element (args arg)
	(make paragraph
		(mktag "li")
		(process-children)))

(element (arg type)
	(make sequence
		(mktag "span class=\"argdef\"")
		(process-children)
		(literal " ")))

(element (arg name)
	(make sequence
		(process-children)
		(literal ":")
		(newline)
		(mktag "/span")))

(element (function returns)
	(make display-group
		(mktag "li")
		(literal "Returns:")
		(newline)
		(process-children)))

(element (returns type)
	(make paragraph
		(mktag "p")
		(process-children)
		(newline)))

(element (function errs)
	(make display-group
		(make paragraph
			(mktag "li")
			(literal "Error codes:")
			(newline)
			(mktag "ul")
			(process-children)
			(mktag "/ul"))))

(element (errs err)
	(make paragraph
		(mktag "li")
		(mktag "dl")
		(mktag "dt")
		(process-matching-children 'ecode)
		(mktag "/dt")
		(mktag "dd")
		(process-matching-children 'emean)
		(mktag "/dd")
		(mktag "/dl")
		(mktag "/li")))

(element (errs note)
	(make paragraph
		(mktag "li")
		(mktag "div class=\"note\"")
		(mktag "p")
		(literal "Note:")
		(newline)
		(mktag "/p")
		(process-children)
		(newline)
		(mktag "/div")
		(mktag "/li")))

;; Simple primatives
(element longdesc
	(make display-group
		(process-children)
		(newline)))

(element desc
	(make sequence
		(process-children)
		(newline)))

(element para
	(make paragraph
		(mktag "p")
		(process-children)
		(newline)
		(mktag "/p")))

(element func
	(make sequence
		(mktag "em class=\"funcname\"")
		(process-children)
		(literal "()")
		(newline)
		(mktag "/em")))

;; Hey, this is easy!
(element br
	(make sequence
		(mktag "br")))

;; Not-so-simple primatives
(mode func-reference
	(element function
		(make sequence
			(optag)
			(literal "a href=\"&hash;")
			(literal (attribute-string "NAME"))
			(literal "\"")
			(closetag)
			(process-first-descendant 'name)
			(literal "()")
			(mktag "/a")))
	(element name
		(process-children)))
;; Synhi hack "

(element fref
	(with-mode func-reference
		(process-element-with-id
			(attribute-string "TARGET"))))

(mode data-reference
	(element datatype
		(make sequence
			(optag)
			(literal "a href=\"&hash;")
			(literal (attribute-string "NAME"))
			(literal "\"")
			(closetag)
			(process-first-descendant 'name)
			(mktag "/a")))
	(element name
		(process-children)))
;; Synhi hack "

(element dref
	(with-mode data-reference
		(process-element-with-id
			(attribute-string "TARGET"))))

;; Take a stab at generating a TOC-ish thing
(mode func-list
	(element functions
		(make display-group
			(process-matching-children 'function)))
	(element function
		(make paragraph
			(mktag "li class=\"listlist\"")
			(with-mode func-reference
				(process-element-with-id
					(attribute-string "NAME")))
			(if (equal? (attribute-string "TYPE") "PRIVATE")
				(make sequence
					(mktag "em")
					(literal "(Internal)")
					(mktag "/em"))
				(empty-sosofo)))))

(mode struct-list
	(element datastructs
		(make display-group
			(process-matching-children 'datatype)))
	(element datatype
		(make paragraph
			(mktag "li class=\"listlist\"")
			(with-mode data-reference
				(process-element-with-id
					(attribute-string "NAME")))
			(if (equal? (attribute-string "TYPE") "PRIVATE")
				(make sequence
					(mktag "em")
					(literal "(Internal)")
					(mktag "/em"))
				(empty-sosofo)))))

(define (print-func-list)
	(make display-group
		(mktag "ul")
		(with-mode func-list (process-matching-children 'functions))
		(mktag "/ul")))

(define (print-struct-list)
	(make display-group
		(mktag "ul")
		(with-mode struct-list (process-matching-children 'datastructs))
		(mktag "/ul")))

(define (print-contents-list)
	(make display-group
		(mktag "div class=\"contents\"")
		(mktag "h1")
		(literal "Contents")
		(mktag "/h1")
		(mktag "h2")
		(mktag "a href=\"#datastructs\"")
		(literal "Data structures:")
		(mktag "/a")
		(newline)
		(mktag "/h2")
		(print-struct-list)
		(mktag "h2")
		(mktag "a href=\"#functions\"")
		(literal "Functions:")
		(mktag "/a")
		(newline)
		(mktag "/h2")
		(print-func-list)
		(mktag "/div")))


;; Start fiddling with the public/private distinction
(mode func-pub
	(element function
		(make display-group
			(if (equal? (attribute-string "TYPE") "PUBLIC")
				(make display-group
					(make paragraph
						(optag)
						(literal "li id=\"")
						(literal (attribute-string "NAME"))
						(literal "\"")
						(closetag)
						(newline))
					(make paragraph
						(process-matching-children 'name 'desc))
					(make paragraph
						(mktag "ul")
						(process-matching-children
							'longdesc 'args 'returns 'errs)
						(newline)
						(mktag "/ul")))
				(empty-sosofo)))))

(mode func-priv
	(element function
		(make display-group
			(if (equal? (attribute-string "TYPE") "PRIVATE")
				(make display-group
					(make paragraph
						(optag)
						(literal "li id=\"")
						(literal (attribute-string "NAME"))
						(literal "\"")
						(closetag)
						(newline))
					(make paragraph
						(process-matching-children 'name 'desc))
					(make paragraph
						(mktag "p")
						(mktag "em class=\"privfunc\"")
						(literal
							"This function is intended for internal use only")
						(newline)
						(mktag "/em")
						(mktag "/p"))
					(make paragraph
						(mktag "ul")
						(process-matching-children
							'longdesc 'args 'returns 'errs)
						(newline)
						(mktag "/ul")))
				(empty-sosofo)))))

(mode struct-pub
	(element datatype
		(if (equal? (attribute-string "TYPE") "PUBLIC")
			(make display-group
				(make paragraph
					(optag)
					(literal "li id=\"")
					(literal (attribute-string "NAME"))
					(literal "\"")
					(closetag)
					(newline))
				(make paragraph
					(process-matching-children 'name 'desc))
				(make paragraph
					(mktag "ul")
					(process-matching-children 'note 'member)
					(newline)
					(mktag "/ul")))
			(empty-sosofo))))

(mode struct-priv
	(element datatype
		(if (equal? (attribute-string "TYPE") "PRIVATE")
			(make display-group
				(make paragraph
					(optag)
					(literal "li id=\"")
					(literal (attribute-string "NAME"))
					(literal "\"")
					(closetag)
					(newline))
				(make paragraph
					(process-matching-children 'name 'desc))
				(make paragraph
					(mktag "p")
					(mktag "em class=\"privdata\"")
					(literal "This datatype is intended for internal use only")
					(newline)
					(mktag "/em")
					(mktag "/p"))
				(make paragraph
					(mktag "ul")
					(process-matching-children 'note 'member)
					(newline)
					(mktag "/ul")))
			(empty-sosofo))))

(define (print-func-pub)
	(with-mode func-pub (process-matching-children 'function)))

(define (print-func-priv)
	(with-mode func-priv (process-matching-children 'function)))

(define (print-struct-pub)
	(with-mode struct-pub (process-matching-children 'datatype)))

(define (print-struct-priv)
	(with-mode struct-priv (process-matching-children 'datatype)))
