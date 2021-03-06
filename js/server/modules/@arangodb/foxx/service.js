'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief FoxxService and FoxxContext types
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Alan Plum
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const internal = require('internal');
const assert = require('assert');
const Module = require('module');
const path = require('path');
const fs = require('fs');
const parameterTypes = require('@arangodb/foxx/manager-utils').parameterTypes;
const getReadableName = require('@arangodb/foxx/manager-utils').getReadableName;

const APP_PATH = internal.appPath ? path.resolve(internal.appPath) : undefined;
const STARTUP_PATH = internal.startupPath ? path.resolve(internal.startupPath) : undefined;
const DEV_APP_PATH = internal.devAppPath ? path.resolve(internal.devAppPath) : undefined;

class AppContext {
  constructor(service) {
    this.basePath = path.resolve(service.root, service.path);
    this.comments = [];
    Object.defineProperty(this, '_service', {
      get() {
        return service;
      }
    });
  }

  fileName(filename) {
    return fs.safeJoin(this._prefix, filename);
  }

  file(filename, encoding) {
    return fs.readFileSync(this.fileName(filename), encoding);
  }

  foxxFilename(filename) {
    return this.fileName(filename);
  }

  path(name) {
    return path.join(this._prefix, name);
  }

  collectionName(name) {
    let fqn = (
      this.collectionPrefix
      + name.replace(/[^a-z0-9]/ig, '_').replace(/(^_+|_+$)/g, '').substr(0, 64)
    );
    if (!fqn.length) {
      throw new Error(`Cannot derive collection name from "${name}"`);
    }
    return fqn;
  }

  collection(name) {
    return internal.db._collection(this.collectionName(name));
  }

  get _prefix() {
    return this.basePath;
  }

  get baseUrl() {
    return `/_db/${encodeURIComponent(internal.db._name())}/${this._service.mount.slice(1)}`;
  }

  get collectionPrefix() {
    return this._service.collectionPrefix;
  }

  get mount() {
    return this._service.mount;
  }

  get name() {
    return this._service.name;
  }

  get version() {
    return this._service.version;
  }

  get manifest() {
    return this._service.manifest;
  }

  get isDevelopment() {
    return this._service.isDevelopment;
  }

  get isProduction() {
    return !this.isDevelopment;
  }

  get options() {
    return this._service.options;
  }

  get configuration() {
    return this._service.configuration;
  }

  get dependencies() {
    return this._service.dependencies;
  }
}

Object.defineProperties(AppContext.prototype, {
  comment: {
    value: function (str) {
      this.comments.push(str);
    }
  },
  clearComments: {
    value: function () {
      this.comments.splice(0, this.comments.length);
    }
  }
});

function createConfiguration(definitions) {
  const config = {};
  Object.keys(definitions).forEach(function (name) {
    const def = definitions[name];
    if (def.default !== undefined) {
      config[name] = def.default;
    }
  });
  return config;
}

function createDependencies(definitions, options) {
  const deps = {};
  Object.keys(definitions).forEach(function (name) {
    Object.defineProperty(deps, name, {
      configurable: true,
      enumerable: true,
      get() {
        const mount = options[name];
        return mount ? require('@arangodb/foxx').getExports(mount) : null;
      }
    });
  });
  return deps;
}

class FoxxService {
  constructor(data) {
    assert(data, 'no arguments');
    assert(data.mount, 'mount path required');
    assert(data.path, `local path required for app "${data.mount}"`);
    assert(data.manifest, `called without manifest for app "${data.mount}"`);

    // mount paths always start with a slash
    this.mount = data.mount;
    this.path = data.path;
    this.isDevelopment = data.isDevelopment || false;

    this.manifest = data.manifest;
    if (!this.manifest.dependencies) {
      this.manifest.dependencies = {};
    }
    if (!this.manifest.configuration) {
      this.manifest.configuration = {};
    }

    this.options = data.options;
    if (!this.options.configuration) {
      this.options.configuration = {};
    }
    if (!this.options.dependencies) {
      this.options.dependencies = {};
    }

    this.configuration = createConfiguration(this.manifest.configuration);
    this.dependencies = createDependencies(this.manifest.dependencies, this.options.dependencies);
    let warnings = this.applyConfiguration(this.options.configuration);
    if (warnings.length) {
      console.warnLines(
        `Stored configuration for app "${data.mount}" has errors:\n${warnings.join('\n  ')}`
      );
    }
    // don't need to apply deps from options -- they work automatically

    if (this.manifest.thumbnail) {
      let thumb = path.resolve(this.root, this.path, this.manifest.thumbnail);
      try {
        this.thumbnail = fs.read64(thumb);
      } catch (e) {
        this.thumbnail = null;
        /*
        console.warnLines(
          `Cannot read thumbnail "${thumb}" for app "${data.mount}": ${e.stack}`
        );
        */
      }
    } else {
      this.thumbnail = null;
    }

    let lib = this.manifest.lib || '.';
    let moduleRoot = path.resolve(this.root, this.path, lib);
    this.moduleCache = {};
    this.main = new Module(`foxx:${data.mount}`, undefined, this.moduleCache);
    this.main.root = moduleRoot;
    this.main.filename = path.resolve(moduleRoot, '.foxx');
    this.main.context.applicationContext = new AppContext(this);
    this.main.context.console = require('@arangodb/foxx/console')(this.mount);
  }

  applyConfiguration(config) {
    const definitions = this.manifest.configuration;
    const warnings = [];

    _.each(config, function (rawValue, name) {
      const def = definitions[name];
      if (!def) {
        warnings.push(`Unexpected option "${name}"`);
        return;
      }

      if (def.required === false && (rawValue === undefined || rawValue === null || rawValue === '')) {
        delete this.options.configuration[name];
        this.configuration[name] = def.default;
        return;
      }

      const validate = parameterTypes[def.type];
      let parsedValue = rawValue;
      let warning;

      if (validate.isJoi) {
        let result = validate.required().validate(rawValue);
        if (result.error) {
          warning = result.error.message.replace(/^"value"/, `"${name}"`);
        } else {
          parsedValue = result.value;
        }
      } else {
        try {
          parsedValue = validate(rawValue);
        } catch (e) {
          warning = `"${name}": ${e.message}`;
        }
      }

      if (warning) {
        warnings.push(warning);
      } else {
        this.options.configuration[name] = rawValue;
        this.configuration[name] = parsedValue;
      }
    }, this);

    return warnings;
  }

  applyDependencies(deps) {
    var definitions = this.manifest.dependencies;
    var warnings = [];

    _.each(deps, function (mount, name) {
      const dfn = definitions[name];
      if (!dfn) {
        warnings.push(`Unexpected dependency "${name}"`);
      } else {
        this.options.dependencies[name] = mount;
      }
    }, this);

    return warnings;
  }

  _PRINT(context) {
    context.output += `[FoxxService "${this.name}" (${this.version}) on ${this.mount}]`;
  }

  toJSON() {
    const result = {
      name: this.name,
      version: this.version,
      manifest: this.manifest,
      path: this.path,
      options: this.options,
      mount: this.mount,
      root: this.root,
      isSystem: this.isSystem,
      isDevelopment: this.isDevelopment
    };
    if (this.error) {
      result.error = this.error;
    }
    if (this.manifest.author) {
      result.author = this.manifest.author;
    }
    if (this.manifest.description) {
      result.description = this.manifest.description;
    }
    if (this.thumbnail) {
      result.thumbnail = this.thumbnail;
    }
    return result;
  }

  simpleJSON() {
    return {
      name: this.name,
      version: this.version,
      mount: this.mount
    };
  }

  development(isDevelopment) {
    this.isDevelopment = isDevelopment;
  }

  getConfiguration(simple) {
    var config = {};
    var definitions = this.manifest.configuration;
    var options = this.options.configuration;
    _.each(definitions, function (dfn, name) {
      var value = options[name] === undefined ? dfn.default : options[name];
      config[name] = simple ? value : _.extend({}, dfn, {
        title: getReadableName(name),
        current: value
      });
    });
    return config;
  }

  getDependencies(simple) {
    var deps = {};
    var definitions = this.manifest.dependencies;
    var options = this.options.dependencies;
    _.each(definitions, function (dfn, name) {
      deps[name] = simple ? options[name] : {
        definition: dfn,
        title: getReadableName(name),
        current: options[name]
      };
    });
    return deps;
  }

  needsConfiguration() {
    var config = this.getConfiguration();
    var deps = this.getDependencies();
    return _.any(config, function (cfg) {
      return cfg.current === undefined && cfg.required !== false;
    }) || _.any(deps, function (dep) {
      return dep.current === undefined && dep.definition.required !== false;
    });
  }

  run(filename, options) {
    options = options || {};
    filename = path.resolve(this.main.context.__dirname, filename);

    var module = new Module(filename, this.main);
    module.context.console = this.main.context.console;
    module.context.applicationContext = _.extend(
      new AppContext(this.main.context.applicationContext._service),
      this.main.context.applicationContext,
      options.appContext
    );

    if (options.preprocess) {
      module.preprocess = options.preprocess;
    }

    if (options.context) {
      Object.keys(options.context).forEach(function (key) {
        module.context[key] = options.context[key];
      });
    }

    module.load(filename);
    return module.exports;
  }

  get exports() {
    return this.main.exports;
  }

  get name() {
    return this.manifest.name;
  }

  get version() {
    return this.manifest.version;
  }

  get isSystem() {
    return this.mount.charAt(1) === '_';
  }

  get root() {
    if (this.isSystem) {
      return FoxxService._systemAppPath;
    }
    return FoxxService._appPath;
  }

  get collectionPrefix() {
    return this.mount.substr(1).replace(/[-.:/]/g, '_') + '_';
  }

  static get _startupPath() {
    return STARTUP_PATH;
  }

  static get _appPath() {
    return APP_PATH ? (
      path.join(APP_PATH, '_db', internal.db._name())
    ) : undefined;
  }

  static get _systemAppPath() {
    return APP_PATH ? (
      path.join(STARTUP_PATH, 'apps', 'system')
    ) : undefined;
  }

  static get _oldAppPath() {
    return APP_PATH ? (
      path.join(APP_PATH, 'databases', internal.db._name())
    ) : undefined;
  }

  static get _devAppPath() {
    return DEV_APP_PATH ? (
      path.join(DEV_APP_PATH, 'databases', internal.db._name())
    ) : undefined;
  }
}

module.exports = FoxxService;
