<?php

$req = [ 	'RequestId' => '12345',
		'Prefix' => [ 		[ '' => 'http://localhost/' ],
					[ 'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#' ],
					[ 'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#' ],
					[ 'owl' => 'http://www.w3.org/2002/07/owl#' ],
					[ 'xsd' => 'http://www.w3.org/2001/XMLSchema#' ],
					[ 'foaf' => 'http://xmlns.com/foaf/0.1/' ] ] ];

file_put_contents( 'prefixes.json', json_encode( $req ) );

?>



