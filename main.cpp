// Planificador de horario diario - Parcial 2 ADA
// Backtracking con poda para maximizar prioridad total sin solapamientos
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <array>
#include <set>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>

using namespace std;

// ---------- Constantes de configuracion (no dependen del usuario) ----------
const int GRANULARIDAD = 30;        // salto en minutos entre candidatas generadas
const int DURACION_COMIDA = 30;     // minutos por comida
const int DURACION_DESCANSO_DEF = 45;
const int DURACION_OCIO_DEF = 90;
const int SUENO_INICIAL = 8 * 60;   // 8 horas
const int SUENO_MINIMO = 6 * 60;    // 6 horas, tope inferior
const int PASO_REDUCCION_SUENO = 30;

// ---------- Estructura principal ----------
struct Actividad {
    string nombre;
    string grupo;   // actividad logica a la que pertenece (para no repetir instancias del mismo grupo)
    int inicio;     // minutos desde medianoche
    int fin;
    double prioridad;
    bool fija;      // true solo para Dormir
};

// Tabla de calificaciones: Salud, Satisfaccion, Realizacion, Necesidad, Obligacion
map<string, array<int, 5>> tablaCalificaciones = {
    {"Comer",    {5, 4, 2, 5, 4}},
    {"Dormir",   {5, 3, 1, 5, 5}},
    {"Deporte",  {5, 3, 5, 3, 3}},
    {"Estudio",  {3, 3, 5, 4, 5}},
    {"Descanso", {4, 3, 1, 3, 1}},
    {"Ocio",     {2, 5, 2, 1, 1}}
};

// Pesos fijos por dimension (mismo orden que la tabla), suma = 12
array<int, 5> pesos = {3, 1, 2, 3, 3};

// Calcula la prioridad ponderada de una actividad (promedio, escala 1-5)
double calcularPrioridad(const string& actividad) {
    array<int, 5> cal = tablaCalificaciones[actividad];
    double suma = 0;
    int sumaPesos = 0;
    for (int i = 0; i < 5; i++) {
        suma += cal[i] * pesos[i];
        sumaPesos += pesos[i];
    }
    return suma / sumaPesos;
}

// ---------- Utilidades de tiempo ----------
// "0830" (4 digitos, sin dos puntos) -> minutos desde medianoche, o -1 si esta mal escrita
int horaAMinutos(const string& hhmm) {
    if (hhmm.size() != 4) return -1;
    for (char c : hhmm) if (!isdigit((unsigned char)c)) return -1;
    int h = stoi(hhmm.substr(0, 2));
    int m = stoi(hhmm.substr(2, 2));
    if (h > 23 || m > 59) return -1;
    return h * 60 + m;
}

// minutos desde medianoche -> "HH:MM" para mostrar en pantalla (envuelve el dia con modulo)
string minutosAHora(int minutos) {
    minutos = ((minutos % 1440) + 1440) % 1440;
    int h = minutos / 60;
    int m = minutos % 60;
    stringstream ss;
    ss << setw(2) << setfill('0') << h << ":" << setw(2) << setfill('0') << m;
    return ss.str();
}

// "0830-0900" -> par de minutos (inicio, fin), o {-1,-1} si esta mal escrita
pair<int, int> parsearRango(const string& rango) {
    size_t pos = rango.find('-');
    if (pos == string::npos) return {-1, -1};
    int a = horaAMinutos(rango.substr(0, pos));
    int b = horaAMinutos(rango.substr(pos + 1));
    if (a < 0 || b < 0) return {-1, -1};
    return {a, b};
}

// ---------- Lectura segura de datos ----------
// Repite la pregunta hasta que el usuario escriba un numero entero valido
// Se lee como texto y se revisa entero para no dejar basura a medias en el buffer
// (por ejemplo "1:30" no debe colarse como si fuera el numero 1)
int leerEntero(const string& mensaje) {
    string token;
    while (true) {
        cout << mensaje;
        cin >> token;
        bool valido = !token.empty();
        for (char c : token) {
            if (!isdigit((unsigned char)c)) { valido = false; break; }
        }
        if (valido) return stoi(token);
        cout << "Numero invalido, intenta de nuevo.\n";
    }
}

// Repite la pregunta hasta que el usuario escriba una hora en formato 0830
int leerHora(const string& mensaje) {
    string linea;
    while (true) {
        cout << mensaje;
        cin >> linea;
        int minutos = horaAMinutos(linea);
        if (minutos >= 0) return minutos;
        cout << "Hora invalida, usa el formato 0830 (4 digitos, sin dos puntos).\n";
    }
}

// Repite la pregunta hasta que el usuario escriba un rango en formato 0830-0900
pair<int, int> leerRango(const string& mensaje) {
    string linea;
    while (true) {
        cout << mensaje;
        cin >> linea;
        pair<int, int> rango = parsearRango(linea);
        if (rango.first >= 0 && rango.second >= 0) return rango;
        cout << "Rango invalido, usa el formato 0830-0900 (sin dos puntos).\n";
    }
}

// Revisa si [inicio, fin) se cruza con alguno de los bloqueos del usuario
bool seSolapaConBloqueos(int inicio, int fin, const vector<pair<int, int>>& bloqueos) {
    for (const auto& b : bloqueos) {
        if (inicio < b.second && fin > b.first) return true; // solapamiento clasico de rangos
    }
    return false;
}

// ---------- Generacion de candidatas ----------
// Recorre la ventana [ventanaInicio, ventanaFin) saltando de a GRANULARIDAD minutos
// Salta las candidatas que caigan sobre un horario bloqueado por el usuario
void generarCandidatas(vector<Actividad>& candidatas, const string& grupo, int duracion,
                        int ventanaInicio, int ventanaFin, double prioridad,
                        const vector<pair<int, int>>& bloqueos) {
    for (int inicio = ventanaInicio; inicio + duracion <= ventanaFin; inicio += GRANULARIDAD) {
        if (seSolapaConBloqueos(inicio, inicio + duracion, bloqueos)) continue;
        Actividad a;
        a.nombre = grupo;
        a.grupo = grupo;
        a.inicio = inicio;
        a.fin = inicio + duracion;
        a.prioridad = prioridad;
        a.fija = false;
        candidatas.push_back(a);
    }
}

// Calcula los huecos libres dentro de [desde, hasta) que dejo el horario ya armado
vector<pair<int, int>> calcularHuecos(const vector<Actividad>& horario, int desde, int hasta) {
    vector<pair<int, int>> huecos;
    int cursor = desde;
    for (const Actividad& a : horario) {
        if (a.grupo == "Dormir") continue; // el sueno no es parte de la vigilia
        if (a.inicio > cursor) huecos.push_back({cursor, a.inicio});
        cursor = max(cursor, a.fin);
    }
    if (cursor < hasta) huecos.push_back({cursor, hasta});
    return huecos;
}

// ---------- Backtracking con poda ----------
vector<Actividad> candidatas;      // pool global de instancias candidatas
vector<double> sufMaxPrioridad;    // cota superior: prioridad maxima posible desde el indice i en adelante
vector<Actividad> mejorSeleccion;
double mejorPrioridad = -1;

void backtrack(int i, int finActual, double prioridadActual,
               vector<Actividad>& seleccionActual, set<string>& gruposUsados) {
    // Poda por cota superior: si ni en el mejor caso se supera lo ya encontrado, se corta la rama
    if (prioridadActual + sufMaxPrioridad[i] <= mejorPrioridad) return;

    // Caso base: se recorrieron todas las candidatas
    if (i == (int)candidatas.size()) {
        if (prioridadActual > mejorPrioridad) {
            mejorPrioridad = prioridadActual;
            mejorSeleccion = seleccionActual;
        }
        return;
    }

    // Rama 1: no incluir la candidata i
    backtrack(i + 1, finActual, prioridadActual, seleccionActual, gruposUsados);

    // Rama 2: incluir la candidata i, si no hay solapamiento y su grupo no fue usado ya
    Actividad& act = candidatas[i];
    bool grupoLibre = gruposUsados.find(act.grupo) == gruposUsados.end();
    if (act.inicio >= finActual && grupoLibre) {
        gruposUsados.insert(act.grupo);
        seleccionActual.push_back(act);
        backtrack(i + 1, act.fin, prioridadActual + act.prioridad, seleccionActual, gruposUsados);
        seleccionActual.pop_back();
        gruposUsados.erase(act.grupo);
    }
}

void generarHorario() {
    // Se limpia el estado global por si ya se genero un horario antes en esta misma ejecucion
    candidatas.clear();
    sufMaxPrioridad.clear();
    mejorSeleccion.clear();
    mejorPrioridad = -1;

    // ---------- Entrada del usuario ----------
    // Todas las horas se piden en formato 0830, 4 digitos y sin dos puntos
    cout << "Las horas se escriben en formato 0830, sin dos puntos.\n";

    int horaDespertarManana = leerHora("A que hora te levantas manana, formato HHMM ej: 2030 (8:30 pm): ");
    int horaDespertarSiguiente = leerHora("A que hora te levantas el dia siguiente, formato HHMM ej: 2030 (8:30 pm): ");

    int numBloqueos = leerEntero("Cuantos horarios deseas bloquear, clases, trabajo, etc.? ");
    vector<pair<int, int>> bloqueos;
    for (int i = 0; i < numBloqueos; i++) {
        bloqueos.push_back(leerRango("Hora del bloqueo " + to_string(i + 1) + ", formato 0830-0900: "));
    }

    int numComidas = leerEntero("Numero de comidas al dia: ");
    vector<pair<int, int>> rangosComidas;
    for (int i = 0; i < numComidas; i++) {
        rangosComidas.push_back(leerRango("Rango horario para comida " + to_string(i + 1) + ", formato 0830-0900: "));
    }

    pair<int, int> ventanaDeporte = leerRango("Ventana disponible para entrenar, formato 0830-0900: ");
    int duracionDeporte = leerEntero("Duracion deseada de entrenamiento en minutos: ");
    int duracionEstudio = leerEntero("Minutos de estudio o trabajo independiente: ");

    // ---------- Bloque de sueno y ventana de vigilia ----------
    // El dia siguiente esta 1440 minutos despues, por eso se suma antes de restar
    int horaDespertarSiguienteRaw = horaDespertarSiguiente + 1440;
    int totalCiclo = horaDespertarSiguienteRaw - horaDespertarManana;

    // Se intenta primero con 8 horas de sueno y se reduce hasta el minimo de 6h
    int duracionSueno = SUENO_INICIAL;
    int vigiliaInicio = horaDespertarManana;
    int vigiliaFin, duracionVigilia;
    while (true) {
        duracionVigilia = totalCiclo - duracionSueno;
        vigiliaFin = vigiliaInicio + duracionVigilia; // puede superar 1440, es valido

        int tiempoSolicitado = numComidas * DURACION_COMIDA + duracionDeporte + duracionEstudio
                              + DURACION_DESCANSO_DEF + DURACION_OCIO_DEF;

        if (tiempoSolicitado <= duracionVigilia || duracionSueno <= SUENO_MINIMO) break;
        duracionSueno -= PASO_REDUCCION_SUENO;
        if (duracionSueno < SUENO_MINIMO) duracionSueno = SUENO_MINIMO;
    }

    // Bloque de Dormir: fijo, se agrega directo sin pasar por el backtracking
    Actividad dormir;
    dormir.nombre = "Dormir";
    dormir.grupo = "Dormir";
    dormir.inicio = vigiliaFin;                 // hora en que empieza el sueno
    dormir.fin = vigiliaFin + duracionSueno;     // puede pasar de 1440 (representa el dia siguiente)
    dormir.prioridad = 5.0;                      // prioridad maxima, forzada
    dormir.fija = true;

    // ---------- Generacion de candidatas por grupo ----------
    // Comida y Deporte se recortan a la vigilia: si el usuario declaro un rango
    // que se mete en el horario de sueno, esa parte simplemente no cuenta (evita
    // que una comida quede solapada con Dormir).
    double prioridadComer = calcularPrioridad("Comer");
    for (int i = 0; i < numComidas; i++) {
        string grupo = "Comida " + to_string(i + 1);
        int inicioValido = max(rangosComidas[i].first, vigiliaInicio);
        int finValido = min(rangosComidas[i].second, vigiliaFin);
        generarCandidatas(candidatas, grupo, DURACION_COMIDA,
                           inicioValido, finValido, prioridadComer, bloqueos);
    }

    double prioridadDeporte = calcularPrioridad("Deporte");
    int inicioDeporteValido = max(ventanaDeporte.first, vigiliaInicio);
    int finDeporteValido = min(ventanaDeporte.second, vigiliaFin);
    generarCandidatas(candidatas, "Deporte", duracionDeporte,
                       inicioDeporteValido, finDeporteValido, prioridadDeporte, bloqueos);

    double prioridadEstudio = calcularPrioridad("Estudio");
    generarCandidatas(candidatas, "Estudio", duracionEstudio,
                       vigiliaInicio, vigiliaFin, prioridadEstudio, bloqueos);

    double prioridadDescanso = calcularPrioridad("Descanso");
    generarCandidatas(candidatas, "Descanso", DURACION_DESCANSO_DEF,
                       vigiliaInicio, vigiliaFin, prioridadDescanso, bloqueos);

    double prioridadOcio = calcularPrioridad("Ocio");
    generarCandidatas(candidatas, "Ocio", DURACION_OCIO_DEF,
                       vigiliaInicio, vigiliaFin, prioridadOcio, bloqueos);

    // Lista de todos los grupos que deberian competir por un lugar en el horario
    vector<string> todosLosGrupos;
    for (int i = 0; i < numComidas; i++) todosLosGrupos.push_back("Comida " + to_string(i + 1));
    todosLosGrupos.push_back("Deporte");
    todosLosGrupos.push_back("Estudio");
    todosLosGrupos.push_back("Descanso");
    todosLosGrupos.push_back("Ocio");

    // ---------- Orden y calculo de la cota superior por sufijo ----------
    // Se ordena por hora de fin, requisito del backtracking tipo "actividad con mayor compatibilidad"
    sort(candidatas.begin(), candidatas.end(), [](const Actividad& a, const Actividad& b) {
        return a.fin < b.fin;
    });

    int n = candidatas.size();
    sufMaxPrioridad.assign(n + 1, 0);
    {
        // Recorrido de atras hacia adelante sumando una sola vez la prioridad de cada grupo visto
        set<string> vistos;
        double sumaAcumulada = 0;
        for (int i = n - 1; i >= 0; i--) {
            if (vistos.find(candidatas[i].grupo) == vistos.end()) {
                vistos.insert(candidatas[i].grupo);
                sumaAcumulada += candidatas[i].prioridad;
            }
            sufMaxPrioridad[i] = sumaAcumulada;
        }
    }

    // ---------- Backtracking ----------
    vector<Actividad> seleccionActual;
    set<string> gruposUsados;
    backtrack(0, vigiliaInicio, 0.0, seleccionActual, gruposUsados);

    // ---------- Armado del horario final ----------
    vector<Actividad> horarioFinal = mejorSeleccion;
    horarioFinal.push_back(dormir);

    // Los bloqueos tambien se muestran en el horario, como ocupados fijos sin prioridad
    for (int i = 0; i < numBloqueos; i++) {
        Actividad b;
        b.nombre = "Bloqueo " + to_string(i + 1);
        b.grupo = b.nombre;
        b.inicio = bloqueos[i].first;
        b.fin = bloqueos[i].second;
        b.prioridad = 0;
        b.fija = true;
        horarioFinal.push_back(b);
    }

    sort(horarioFinal.begin(), horarioFinal.end(), [](const Actividad& a, const Actividad& b) {
        return a.inicio < b.inicio;
    });

    // Grupos que ya quedaron cubiertos en la mejor seleccion
    set<string> gruposCubiertos;
    for (const Actividad& a : mejorSeleccion) gruposCubiertos.insert(a.grupo);

    // ---------- Relleno de huecos para Estudio, Descanso y Ocio ----------
    // Si la duracion exacta pedida no cabe en ningun lado, se agrega igual una
    // version mas corta en el hueco libre mas grande, en vez de excluir la
    // actividad por completo. Comida y Deporte no entran aca a proposito.
    const int UMBRAL_MINIMO_RELLENO = 15; // minutos minimos para que valga la pena rellenar

    struct InfoRelleno { string grupo; int duracion; double prioridad; };
    vector<InfoRelleno> pendientes = {
        {"Estudio", duracionEstudio, prioridadEstudio},
        {"Descanso", DURACION_DESCANSO_DEF, prioridadDescanso},
        {"Ocio", DURACION_OCIO_DEF, prioridadOcio}
    };

    map<string, pair<int, int>> parciales; // grupo -> (minutos asignados, minutos pedidos)

    for (const InfoRelleno& info : pendientes) {
        if (gruposCubiertos.count(info.grupo)) continue; // ya tiene su bloque completo

        vector<pair<int, int>> huecos = calcularHuecos(horarioFinal, vigiliaInicio, vigiliaFin);

        // Ciclo buscador: se queda con el hueco mas grande disponible
        int mejorIni = -1, mejorTam = 0;
        for (auto& h : huecos) {
            int tam = h.second - h.first;
            if (tam > mejorTam) { mejorTam = tam; mejorIni = h.first; }
        }

        if (mejorTam >= UMBRAL_MINIMO_RELLENO) {
            int duracionAsignada = min(info.duracion, mejorTam);
            Actividad relleno;
            relleno.nombre = info.grupo;
            relleno.grupo = info.grupo;
            relleno.inicio = mejorIni;
            relleno.fin = mejorIni + duracionAsignada;
            relleno.prioridad = info.prioridad;
            relleno.fija = false;

            horarioFinal.push_back(relleno);
            sort(horarioFinal.begin(), horarioFinal.end(), [](const Actividad& a, const Actividad& b) {
                return a.inicio < b.inicio;
            });
            gruposCubiertos.insert(info.grupo);
            if (duracionAsignada < info.duracion) parciales[info.grupo] = {duracionAsignada, info.duracion};
        }
    }

    // ---------- Salida ----------
    cout << "\n=== Horario del dia ===\n";
    for (const Actividad& a : horarioFinal) {
        cout << a.nombre << ": " << minutosAHora(a.inicio) << " - " << minutosAHora(a.fin);
        if (a.grupo.rfind("Bloqueo", 0) == 0) {
            cout << " (bloqueado por el usuario)";
        } else if (parciales.count(a.grupo)) {
            cout << " [parcial: " << parciales[a.grupo].first << "/" << parciales[a.grupo].second << " min]";
        }
        cout << "\n";
    }

    vector<string> excluidas;
    for (const string& g : todosLosGrupos) {
        if (gruposCubiertos.find(g) == gruposCubiertos.end()) excluidas.push_back(g);
    }

    if (excluidas.empty()) {
        cout << "Todas las actividades fueron incluidas en el horario.\n";
    } else {
        cout << "No fue posible incluir: ";
        for (size_t i = 0; i < excluidas.size(); i++) {
            cout << excluidas[i];
            if (i + 1 < excluidas.size()) cout << ", ";
        }
        cout << ", por conflicto de horario con las actividades de mayor prioridad.\n";
    }
}

// ---------- Menu principal ----------
int main() {
    while (true) {
        cout << "\n===== Planificador de horario diario =====\n";
        cout << "1. Generar horario del dia\n";
        cout << "2. Salir\n";
        int opcion = leerEntero("Elige una opcion: ");

        if (opcion == 1) {
            generarHorario();
        } else if (opcion == 2) {
            break;
        } else {
            cout << "Opcion invalida, intenta de nuevo.\n";
        }
    }
    return 0;
}
