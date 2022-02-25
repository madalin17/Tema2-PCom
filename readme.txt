## Dobrita George-Madalin -- 324CAb
## Tema2 - PCom
## Aplicatie client-server TCP si UDP pentru gestionarea mesajelor

helpers.h: Fiser header pentru macro-ul DIE, constante int si string si
definire de structuri precum:
	* struct message: folosit pentru a parsa mesajele primite de la clienti
UDP si a le da forward la clientii TCP pentru interpretare si afisare; contine
topicul, tipul de date si datele(face comunicarea client udp->server).
	* struct fromupd: face comunicarea server->client tcp si ii trimite
clientului ip-ul si portul clientului udp si un struct message
	* struct subject: folosit pentru comunicarea de la clientii TCP la server;
mereu se trimite un astfel de mesaj la comunicarea client TCP->server; contine
sf-ul, id-ul clientului si topicul. In functie de sf reiese tipul de mesaj:
		- de conectare*: sf = INT_MAX, id setat, topic nul
		- de deconectare**: sf = INT_MIN, id setat, topic nul
	-> aceste doua tipuri de mesaje sunt folosite la conectarea.deconectarea
unui client pentru a stii serverul ce sa printeze(id-ul clientului conectat/
deconectat)
		- de subscribe: sf = 0/1, id setat, topic setat
		- de unsubscribe : sf = -1, id setat, topic setat

		
server.cpp: Fisierul incepe cu declararea datelor printre care 5 map-uri:
	- topic_map: contine perechi (topic, alt map), iar map-ul din interior
contine perechi (id, sf), adica toti clientii abonati la acel topic
	- sock_map: contine perechi (id, socket de pe care s-a primit id-ul)
	- connected_map: contine perechi (id, 1/0), 1 daca clientul cu acest id
este conectat, 0 altfel
	- queue_map: contine perechi (id, coada de pointeri la structuri de tip
struct message pentru a fi redirectionate cand se reconecteaza clientul pentru
topicuri la care este abonat cu sf-ul 1)
	- ip_port_map: contine perechi (socket, struct sockaddr_in care contine
ip-ul si port-ul socket-ului din pereche)
	Se reseteaza set-urile de socketi si se deschid socket-urile UDP si TCP
pentru portul dat parametru, se face bind(si listen pentru portul TCP), apoi se
da set socketilor si lui 0(citirea de la stdin) in set. Se seteaza socketul
maxim.

	Intr-o bucla:
	- daca 0 este setat(se citeste de la stdin), atunci daca se citeste
stringul "exit", atunci sunt anuntati toti clientii activi sa isi inchida
socketii, serverul inchide socketul de comunicare cu fiecare din acestia, apoi
se iese din bucla si se inchide serverul
	- daca socket-ul UDP este setat, se citesc date de la un client udp, iar
daca niciun client nu este abonat la acest topic, se trece peste trimiterea
mesajului; altfel, se ia fiecare pereche (id, sf) din map-ul topicului citit de
pe socket; daca clientul id este conectat, se trimite direct mesajul. Daca
clientul nu este conectat si sf-ul este 1, se aloca un struct fromudp, se
copiaza bufferul, ip-ul si portul il el si se adauga in coada clientului id
	- daca socket-ul TCP este setat, se incearca acceptarea unui nou socket
si se da update socketului maxim din set; la final se adauga in ip_port_map
perechea dintre socket-ul acceptat si struct sockaddr_in-ul rezultat din
operatia de accept(care contine ip-ul si portul socketului/clientului)
	- altfel, se primesc date de la un client TCP(un struct subject*). In
functie de sf-ul primit:
		- INT_MAX: daca cheia id din structura nu exista in sock_map,
este prima oara cand clientul cu acest id se conecteaza si se afiseaza mesajul
de "New client...". Daca clietul nu este conectat inainte de primirea mesajului,
nu este prima oara cand se conecteaza, deci se afiseaza acelasi mesaj, se
conecteaza clientul si se trimit toate mesajele din coada clientului id(daca
exista). Altfel, se conecteaza ul alt client cu acelasi id la server, deci se
afiseaza "Client ... already connected" si se trimite mesaj clientului sa se
inchida, scotandu-l in din set si inchizandu-l
		- INT_MIN: se deconecteaza clientul cu acest socket si id
		- -1: clientul id se deconecteaza de la topicul t, deci din
intrarea t a topic_map-ului se scoate din map-ul interior cheia id
		- altfel, se aboneaza cu id-ul si sf-ul din structura clientul
curent la topicul t din structura


subscribe.cpp: Se reseteaza set-urile de socketi la care se adauga 0(citirea de
de la stdin) si socket-ul TCP deschis pentru comunicarea cu serverul. Inainte de
o bocla infinita se trimite un mesaj de conectare* serverului

	Intr-o bucla:
	- daca 0 este setat(se citeste de la stdin), atunci daca mesajul este de
tip subscribe se seteaza topicul si sf-ul, daca mesajul este de tip unsubscribe
se seteaza topicul si sf-ul -1, daca mesajul este de tip exit, se inchide
clientul si se anunta serverul printr-un mesaj de deconectare**. Pentru primele
doua cazuri, se seteaza toate campurile de mai devreme intr-un struct subject*
care se trimite spre server, iar apoi in fuctie de sf se afiseaza la tastatura
unul din mesajele "Unsubscribed from topic."/"Subscribed to topic."
	- daca socket-ul TCP este setat, daca se primeste mesajul "exit", se
iese din bucla si se inchide clientul(fortat). Altfel, s-a primit un struct
fromudp* de la server. In functie de data_type-ul din structura se afiseaza
un mesaj la stdout(sunt mai multe detalii in scheletul de cod si enuntul temei).

