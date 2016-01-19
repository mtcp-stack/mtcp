<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN">

;; <!-- $Id: print.dsl,v 1.10 2003/07/15 19:06:41 fullermd Exp $ -->

;; DSSSL info for transformations
(declare-initial-value font-family-name "serif")
(declare-initial-value font-size 14pt)
(declare-initial-value min-leading 0pt)
(define %default-margin% 1in)
(define %default-fontsize% 14pt)
(define %font-heading% (* %default-fontsize% 1.5))
(define %font-subheading% (* %default-fontsize% 1.25))
(define %empty-line% (* %default-fontsize% .75))

;; Outside-in
(element codelibrary
	(make simple-page-sequence
		left-margin: %default-margin%
		right-margin: %default-margin%
		top-margin: %default-margin%
		bottom-margin: %default-margin%
		(process-matching-children 'geninfo)
		(print-contents-list)
		(process-matching-children 'datastructs)
		(process-matching-children 'functions)))

(element geninfo
	(make simple-page-sequence
		(process-children)))

(element (geninfo longdesc)
	(make simple-page-sequence
		(make paragraph
			space-after: %empty-line%
			font-size: %font-heading%
			font-weight: 'bold
			(literal "  Summary:"))
		(make paragraph
			space-after: %empty-line%
			(empty-sosofo))
		(process-children)))

(element datastructs
	(make simple-page-sequence
		(if (first-sibling?)
			(make paragraph
				space-after: (* %empty-line% 2)
				font-size: %font-heading%
				font-weight: 'bold
				(literal "  Data structures:"))
			(empty-sosofo))
		(print-struct-pub)
		(print-struct-priv)))

;;(element datatype
;; In pub/priv modes

(element (datatype name)
	(make sequence
		font-size: %font-subheading%
		font-weight: 'medium
		(literal "- ")
		(process-children)
		(literal ":    ")))

(element (datatype note)
	(make sequence
		start-indent: 10pt
		end-indent: 10pt
		(make paragraph
			keep-with-next?: #t
			(literal "Note:"))
		(make paragraph
			start-indent: 15pt
			end-indent: 15pt
			space-before: %empty-line%
			(process-children))))

(element member
	(make sequence
		start-indent: 10pt
		end-indent: 10pt
		font-weight: 'medium
		(if (first-sibling?)
			(make paragraph
				(literal "Members:"))
			(empty-sosofo))
		(make paragraph
			start-indent: 15pt
			end-indent: 15pt
			space-before: %empty-line%
			(process-children))))

;; Fuck.  I hate hard-coding and limiting shit like this.
;; Oh well, I can't think of any reason (famous last words) for
;; more than 1 level of sub-grouping here.
(element (member member)
	(make paragraph
		start-indent: 20pt
		end-indent: 20pt
		font-weight: 'medium
		space-before: %empty-line%
		(process-children)))

(element (member type)
	(make sequence
		font-weight: 'medium
		(literal "- ")
		(process-children)
		(literal " ")))

(element (member name)
	(make sequence
		font-weight: 'medium
		(process-children)
		(literal ":    ")))

(element functions
	(make simple-page-sequence
		(if (first-sibling?)
			(make paragraph
				space-after: (* %empty-line% 2)
				font-size: %font-heading%
				font-weight: 'bold
				(literal "  Functions:"))
			(empty-sosofo))
		(print-func-pub)
		(print-func-priv)))

;;(element function
;; Now in pub/priv modes

(element (function name)
	(make sequence
		font-size: %font-subheading%
		font-weight: 'medium
		(literal "- ")
		(process-children)
		(literal "():   ")))

(element (function longdesc)
	(make sequence
		(make paragraph
			start-indent: 10pt
			end-indent: 10pt
			space-after: (* %empty-line% .5)
			font-size: %font-subheading%
			font-weight: 'medium
			keep-with-next?: #t
			(literal "Summary:"))
		(make paragraph
			start-indent: 15pt
			end-indent: 15pt
			(process-children))))

(element args
	(make sequence
		(make paragraph
			start-indent: 10pt
			end-indent: 10pt
			space-after: (* %empty-line% .5)
			font-size: %font-subheading%
			font-weight: 'medium
			keep-with-next?: #t
			(literal "Arguments:"))
		(make paragraph
			start-indent: 15pt
			end-indent: 15pt
			space-after: %empty-line%
			(if (= (node-list-length (children (current-node))) 0)
				(make paragraph
					(literal "None."))
				(process-children)))))

(element arg
	(make paragraph
		(process-children)))

(element (arg type)
	(make sequence
		(literal "- ")
		(process-children)
		(literal " ")))

(element (arg name)
	(make sequence
		(process-children)
		(literal ":    ")))

(element returns
	(make sequence
		(make paragraph
			start-indent: 10pt
			end-indent: 10pt
			space-after: (* %empty-line% .5)
			font-size: %font-subheading%
			font-weight: 'medium
			keep-with-next?: #t
			(literal "Return value:"))
		(make paragraph
			start-indent: 15pt
			end-indent: 15pt
			(process-children))))

(element (returns type)
	(make paragraph
		space-after: (* %empty-line% .5)
		font-weight: 'medium
		(process-children)))

(element (returns longdesc)
	(make paragraph
		start-indent: 20pt
		end-indent: 20pt
		font-weight: 'medium
		(process-children)))

(element (function errs)
	(make sequence
		(make paragraph
			start-indent: 10pt
			end-indent: 10pt
			space-after: (* %empty-line% .5)
			font-size: %font-subheading%
			font-weight: 'medium
			keep-with-next?: #t
			(literal "Error codes:"))
		(make paragraph
			start-indent: 15pt
			end-indent: 15pt
			(process-children))))

(element (errs err)
	(make sequence
		(make paragraph
			keep-with-next?: #t
			(literal "- [")
			(process-matching-children 'ecode)
			(literal "]"))
		(make paragraph
			start-indent: 40pt
			end-indent: 40pt
			(process-matching-children 'emean))))

(element (errs note)
	(make sequence
		(make paragraph
			keep-with-next?: #t
			(literal "Note:"))
		(make paragraph
			start-indent: 40pt
			end-indent: 40pt
			space-before: %empty-line%
			(process-children))))

;; 'Internal'-type elements
(element name
	(make sequence
		font-weight: 'bold
		(process-children)))

(element type
	(make paragraph
		(process-children)))

(element desc
	(make sequence
		(process-children)))

(element longdesc
	(make paragraph
		(process-children)))


;; Paragraphs become paragraphs
(element para
	(make paragraph
		space-after: (* %empty-line% .5)
		(process-children)))

(element func
	(make sequence
		font-weight: 'bold
		(process-children)
		(literal "()")))

;; I wonder if this will actually work...
;; If it does, it's a hack and a half.  Oh well.
(element br
	(make display-group
		(empty-sosofo)))

;; Work on these references
(mode func-reference
	(element function
		(make sequence
			font-weight: 'bold
			(process-first-descendant 'name)
			(literal "()")))
	(element name
		(process-children)))

(element fref
	(with-mode func-reference
		(process-element-with-id
			(attribute-string "TARGET"))))

(mode data-reference
	(element datatype
		(make sequence
			font-weight: 'bold
			(process-first-descendant 'name)))
	(element name
		(process-children)))

(element dref
	(with-mode data-reference
		(process-element-with-id
			(attribute-string "TARGET"))))

;; A TOC-ish thing
(mode func-list
	(element functions
		(make display-group
			(process-matching-children 'function)))
	(element function
		(make paragraph
			start-indent: 20pt
			end-indent: 20pt
			space-before: (* %empty-line% .25)
			(with-mode func-reference
				(process-element-with-id
					(attribute-string "NAME")))
			(if (equal? (attribute-string "TYPE") "PRIVATE")
				(make sequence
					font-size: (* %default-fontsize% .8)
					(literal " (Internal)"))
				(empty-sosofo)))))

(mode struct-list
	(element datastructs
		(make display-group
			(process-matching-children 'datatype)))
	(element datatype
		(make paragraph
			start-indent: 20pt
			end-indent: 20pt
			space-before: (* %empty-line% .25)
			(with-mode data-reference
				(process-element-with-id
					(attribute-string "NAME")))
			(if (equal? (attribute-string "TYPE") "PRIVATE")
				(make sequence
					font-size: (* %default-fontsize% .8)
					(literal " (Internal)"))
				(empty-sosofo)))))

(define (print-func-list)
	(make paragraph
		(with-mode func-list (process-matching-children 'functions))))

(define (print-struct-list)
	(make paragraph
		(with-mode struct-list (process-matching-children 'datastructs))))

(define (print-contents-list)
	(make simple-page-sequence
		(make paragraph
			start-indent: 10pt
			end-indent: 10pt
			font-weight: 'bold
			font-size: %font-heading%
			(literal "Contents"))
		(make paragraph
			start-indent: 15pt
			end-indent: 15pt
			font-weight: 'bold
			font-size: %font-subheading%
			space-before: %empty-line%
			(literal "Data structures:"))
		(print-struct-list)
		(make paragraph
			start-indent: 15pt
			end-indent: 15pt
			font-weight: 'bold
			font-size: %font-subheading%
			space-before: %empty-line%
			(literal "Functions:"))
		(print-func-list)))


;; Modes for public/private distinction
(mode func-pub
	(element function
		(if (equal? (attribute-string "TYPE") "PUBLIC")
			(make sequence
				(make paragraph
					keep-with-next?: #t
					space-after: (* %empty-line% .5)
					(process-matching-children 'name 'desc))
				(make paragraph
					space-after: %empty-line%
					(process-matching-children 'longdesc 'args 'returns 'errs))
				(make paragraph
					space-after: %empty-line%
					(empty-sosofo)))
			(empty-sosofo))))

(mode func-priv
	(element function
		(if (equal? (attribute-string "TYPE") "PRIVATE")
			(make sequence
				(make paragraph
					keep-with-next?: #t
					space-after: (* %empty-line% .5)
					(process-matching-children 'name 'desc))
				(make paragraph
					keep-with-next?: #t
					font-size: %font-subheading%
					font-weight: 'medium
					start-indent: 20pt
					end-indent: 20pt
					(literal "*** This function is for internal use only ***"))
				(make paragraph
					space-after: %empty-line%
					(process-matching-children 'longdesc 'args 'returns 'errs))
				(make paragraph
					space-after: %empty-line%
					(empty-sosofo)))
			(empty-sosofo))))

(mode struct-pub
	(element datatype
		(if (equal? (attribute-string "TYPE") "PUBLIC")
			(make sequence
				(make paragraph
					keep-with-next?: #t
					space-after: (* %empty-line% .5)
					(process-matching-children 'name 'desc))
				(make paragraph
					space-after: %empty-line%
					(process-matching-children 'note 'member))
				(make paragraph
					space-after: %empty-line%
					(empty-sosofo)))
			(empty-sosofo))))

(mode struct-priv
	(element datatype
		(if (equal? (attribute-string "TYPE") "PRIVATE")
			(make sequence
				(make paragraph
					keep-with-next?: #t
					space-after: (* %empty-line% .5)
					(process-matching-children 'name 'desc))
				(make paragraph
					keep-with-next?: #t
					font-size: %font-subheading%
					font-weight: 'medium
					start-indent: 20pt
					end-indent: 20pt
					(literal "*** This datatype is for internal use only ***"))
				(make paragraph
					space-after: %empty-line%
					(process-matching-children 'note 'member))
				(make paragraph
					space-after: %empty-line%
					(empty-sosofo)))
			(empty-sosofo))))

(define (print-func-pub)
	(with-mode func-pub (process-matching-children 'function)))

(define (print-func-priv)
	(with-mode func-priv (process-matching-children 'function)))

(define (print-struct-pub)
	(with-mode struct-pub (process-matching-children 'datatype)))

(define (print-struct-priv)
	(with-mode struct-priv (process-matching-children 'datatype)))
