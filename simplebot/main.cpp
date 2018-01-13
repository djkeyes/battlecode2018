
#include <iostream>
#include <list>
#include <map>

#include "bcpp_api/bc.hpp"
#include "Util.h"

#ifdef DEBUG
#  define LOG(x) do { std::cout << x; } while(0)
#else
#  define LOG(x)
#endif

using namespace bc;
using std::vector;
using std::map;
using std::list;
using std::endl;

class Bot {
public:
    Bot(GameController &gc) : m_gc(gc) {
        // nothing for now
    }

    void turn() {
        const vector<Unit> units(m_gc.get_my_units());

        map<bc_UnitType, list<const Unit *>> units_by_type;

        for (auto &unit : units) {
            units_by_type[unit.get_unit_type()].push_back(&unit);
        }

        bool has_factory = units_by_type.find(UnitType::Factory) != units_by_type.end();
        // no factories? build one
        if (!has_factory) {
            has_factory = tryBlueprintingFactory(units_by_type[UnitType::Worker]);
        }

        tryBuildingFactories(units_by_type[UnitType::Worker]);

        bool has_knight = units_by_type.find(UnitType::Knight) != units_by_type.end();
        if (has_factory) {
            // is it possible to use the factory on the same turn it was built? If so, how do we get the robotid?
            // note to self: should probably consider queued units when deciding whether to build more units.
            has_knight |= tryBuildingKnight();
        }

        if (has_knight) {
            // do stuff with the knights
            moveKnightsTowardEnemy();
        }
    }

    void tryBuildingFactories(const list<const Unit *> workers) {
        // check if any buildings are under construction
        list<unsigned int> finished;
        if (m_buildings_under_construction_to_workers.size() > 0) {
            bool any_factories_finished = false;
            for (const auto &site : m_buildings_under_construction_to_workers) {
                const Unit blueprint = m_gc.get_unit(site.first);
                for (const auto &worker_id : site.second) {
                    // assuming the worker hasn't moved, this should only check the has_acted state
                    if (!m_gc.can_build(worker_id, site.first)) {
                        continue;
                    }
                    m_gc.build(worker_id, site.first);
                    if (blueprint.structure_is_built()) {
                        finished.push_back(site.first);
                        break; // all done!
                    }
                }
            }
        }
        for (const auto &element : finished) {
            m_buildings_under_construction_to_workers.erase(element);
        }

        // maybe have nearby workers path toward factories? and add them to the list once they're close enough?
    }

    bool tryBlueprintingFactory(const list<const Unit *> workers) {
        // try to build a factory with each worker
        if (m_gc.get_karbonite() < unit_type_get_blueprint_cost(UnitType::Factory)) {
            return false;
        }
        for (const auto worker : workers) {
            unsigned int worker_id(worker->get_id());
            for (const auto &d : directions) {
                if (m_gc.can_blueprint(worker_id, UnitType::Factory, d)) {
                    m_gc.blueprint(worker_id, UnitType::Factory, d);
                    Unit blueprint = m_gc.sense_unit_at_location(worker->get_location().get_map_location().add(d));
                    m_buildings_under_construction_to_workers[blueprint.get_id()].push_back(worker_id);
                }
            }
        }

        return false;
    }

    bool tryBuildingKnight() {
        return false;
    }

    void moveKnightsTowardEnemy() {

    }

private:
    GameController &m_gc;

    map<unsigned int, list<unsigned int>> m_buildings_under_construction_to_workers;

};

int main(int argv, char **argc) {

    srand(0);

    GameController gc;
    Bot bot(gc);

    CHECK_ERRORS();

    // loop through the whole game.
    while (true) {
        LOG("current turn: " << gc.get_round() << endl);
        bot.turn();

        CHECK_ERRORS()

        fflush(stdout);
        gc.next_turn();
    }
}