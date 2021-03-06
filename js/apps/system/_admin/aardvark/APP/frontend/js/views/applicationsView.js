/*jshint browser: true */
/*global Backbone, $, window, arangoHelper, templateEngine, _*/
(function() {
  "use strict";

  window.ApplicationsView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("applicationsView.ejs"),

    events: {
      "click #addApp"                : "createInstallModal",
      "click #foxxToggle"            : "slideToggle",
      "click #checkDevel"            : "toggleDevel",
      "click #checkProduction"       : "toggleProduction",
      "click #checkSystem"           : "toggleSystem"
    },

    fixCheckboxes: function() {
      if (this._showDevel) {
        $('#checkDevel').attr('checked', 'checked');
      }
      else {
        $('#checkDevel').removeAttr('checked');
      }
      if (this._showSystem) {
        $('#checkSystem').attr('checked', 'checked');
      }
      else {
        $('#checkSystem').removeAttr('checked');
      }
      if (this._showProd) {
        $('#checkProduction').attr('checked', 'checked');
      }
      else {
        $('#checkProduction').removeAttr('checked');
      }
      $('#checkDevel').next().removeClass('fa fa-check-square-o fa-square-o').addClass('fa');
      $('#checkSystem').next().removeClass('fa fa-check-square-o fa-square-o').addClass('fa');
      $('#checkProduction').next().removeClass('fa fa-check-square-o fa-square-o').addClass('fa');
      arangoHelper.setCheckboxStatus('#foxxDropdown');
    },

    toggleDevel: function() {
      var self = this;
      this._showDevel = !this._showDevel;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
      });
      this.fixCheckboxes();
    },

    toggleProduction: function() {
      var self = this;
      this._showProd = !this._showProd;
      _.each(this._installedSubViews, function(v) {
        v.toggle("production", self._showProd);
      });
      this.fixCheckboxes();
    },

    toggleSystem: function() {
      this._showSystem = !this._showSystem;
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("system", self._showSystem);
      });
      this.fixCheckboxes();
    },

    reload: function() {
      var self = this;

      // unbind and remove any unused views
      _.each(this._installedSubViews, function (v) {
        v.undelegateEvents();
      });

      this.collection.fetch({
        success: function() {
          self.createSubViews();
          self.render();
        }
      });
    },

    createSubViews: function() {
      var self = this;
      this._installedSubViews = { };

      self.collection.each(function (foxx) {
        var subView = new window.FoxxActiveView({
          model: foxx,
          appsView: self
        });
        self._installedSubViews[foxx.get('mount')] = subView;
      });
    },

    initialize: function() {
      this._installedSubViews = {};
      this._showDevel = true;
      this._showProd = true;
      this._showSystem = false;
    },

    slideToggle: function() {
      $('#foxxToggle').toggleClass('activated');
      $('#foxxDropdownOut').slideToggle(200);
    },

    createInstallModal: function(event) {
      event.preventDefault();
      window.foxxInstallView.install(this.reload.bind(this));
    },

    render: function() {
      this.collection.sort();

      $(this.el).html(this.template.render({}));
      _.each(this._installedSubViews, function (v) {
        $("#installedList").append(v.render());
      });
      this.delegateEvents();
      $('#checkDevel').attr('checked', this._showDevel);
      $('#checkProduction').attr('checked', this._showProd);
      $('#checkSystem').attr('checked', this._showSystem);
      arangoHelper.setCheckboxStatus("#foxxDropdown");
      
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
        v.toggle("system", self._showSystem);
      });

      arangoHelper.fixTooltips("icon_arangodb", "left");
      return this;
    }

  });

  /* Info for mountpoint
   *
   *
      window.modalView.createTextEntry(
        "mount-point",
        "Mount",
        "",
        "The path the app will be mounted. Has to start with /. Is not allowed to start with /_",
        "/my/app",
        true,
        [
          {
            rule: Joi.string().required(),
            msg: "No mountpoint given."
          },
          {
            rule: Joi.string().regex(/^\/[^_]/),
            msg: "Mountpoints with _ are reserved for internal use."
          },
          {
            rule: Joi.string().regex(/^(\/[a-zA-Z0-9_\-%]+)+$/),
            msg: "Mountpoints have to start with / and can only contain [a-zA-Z0-9_-%]"
          }
        ]
      )
   */
}());
