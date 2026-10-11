#ifndef ATG_ENGINE_SIM_COMPILER_H
#define ATG_ENGINE_SIM_COMPILER_H

#include "language_rules.h"

#include "engine_sim.h"
#include "piranha.h"

#include <ostream>
#include <string>
#include <vector>

namespace es_script {

    class Compiler {
    public:
        struct Output {
            Engine *engine = nullptr;
            Vehicle *vehicle = nullptr;
            Transmission *transmission = nullptr;
            Simulator::Parameters simulatorParameters;
            ApplicationSettings applicationSettings;

            // True if the script called set_application_settings()
            bool applicationSettingsSet = false;

            std::vector<Function *> functions;
        };

    private:
        static Output *s_output;

    public:
        Compiler();
        ~Compiler();

        static Output *output();

        // Search paths are checked in order when resolving imports
        void initialize(const std::vector<std::string> &searchPaths = {});

        // Compilation errors are written to `log`
        bool compile(const piranha::IrPath &path, std::ostream &log);
        Output execute();
        void destroy();

    private:
        void printError(const piranha::CompilationError *err, std::ostream &log) const;

    private:
        LanguageRules m_rules;
        piranha::Compiler *m_compiler;
        piranha::NodeProgram m_program;
    };

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_COMPILER_H */