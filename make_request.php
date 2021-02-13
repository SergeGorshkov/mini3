<?php

$req = [ 	'RequestId' => '12345',
		'Pattern' => [ 	[ 'http://localhost/Serge', 'rdf:type', 'owl:NamedIndividual' ],
				[ 'Serge', 'rdf:type', 'foaf:Person' ],
				[ 'Serge', 'rdfs:label', 'Serge', 'xsd:string' ],
				[ 'Fedya', 'rdf:type', 'owl:NamedIndividual' ],
				[ 'Fedya', 'rdfs:label', 'Fedya', 'xsd:string' ],
				[ 'Fedya', 'foaf:isFriendOf', 'Serge' ],
				[ 'Serge', 'foaf:isFriendOf', 'Fedya' ] ] ];

file_put_contents( 'request.json', json_encode( $req ) );

?>



